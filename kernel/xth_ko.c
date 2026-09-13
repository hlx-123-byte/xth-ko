// SPDX-License-Identifier: GPL-2.0
/*
 * xth_ko - XTH kernel helper (v0.1 research skeleton)
 *
 * 目标内核: 5.10.209-android12-OP-RESUKISU-huangdihd (SM8475 / taro / PHK110)
 *
 * 设计原则:
 *   - 只导入 EXPORT_SYMBOL 的符号，绝不用 kallsyms/未导出符号（v1 不下探）。
 *   - 不做 inline hook、不改代码段、不隐藏模块（v1 骨架保持最小、可回滚）。
 *   - 提供 /dev/xth_ko，ioctl 实现跨进程虚拟内存 read/write。
 *
 * 运行时说明:
 *   该 ReSukiSU 定制内核已被 patch，vermagic / modversions 校验被放行，
 *   因此无需精确匹配内核源码树即可加载（pathmask.ko 是活证据）。
 *
 * 安全:
 *   - 仅内核读写，无持久化、无自隐藏；rmmod 即可完全卸载。
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <linux/ioctl.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xth / haious");
MODULE_DESCRIPTION("XTH kernel helper: cross-process memory r/w (research skeleton)");
MODULE_VERSION("0.1.0");

#define XTH_IOCTL_MAGIC 0x58 /* 'X' */
#define XTH_MAX_LEN (16u * 1024u * 1024u)

struct xth_req {
	__u32 pid;  /* target tgid */
	__u32 op;   /* 0 = read, 1 = write */
	__u64 addr; /* target virtual address */
	__u64 buf;  /* user-space buffer */
	__u32 len;  /* bytes */
	__u32 out;  /* bytes actually transferred */
};

#define XTH_IOC_RW _IOWR(XTH_IOCTL_MAGIC, 0x01, struct xth_req)

static int xth_do_rw(struct xth_req *req)
{
	struct task_struct *task;
	struct mm_struct *mm;
	void *kbuf;
	int done = 0;
	int ret = 0;

	if (req->len == 0 || req->len > XTH_MAX_LEN)
		return -EINVAL;
	if (req->op > 1)
		return -EINVAL;

	task = pid_task(find_vpid(req->pid), PIDTYPE_PID);
	if (!task)
		return -ESRCH;
	get_task_struct(task);

	mm = get_task_mm(task);
	if (!mm) {
		ret = -EFAULT;
		goto out_task;
	}

	kbuf = kmalloc(req->len, GFP_KERNEL);
	if (!kbuf) {
		ret = -ENOMEM;
		goto out_mm;
	}

	if (req->op == 1) { /* write into target */
		if (copy_from_user(kbuf, (void __user *)(unsigned long)req->buf,
				   req->len)) {
			ret = -EFAULT;
			goto out_buf;
		}
		done = access_process_vm(task, (unsigned long)req->addr, kbuf,
					 req->len, FOLL_FORCE | FOLL_WRITE);
	} else { /* read from target */
		done = access_process_vm(task, (unsigned long)req->addr, kbuf,
					 req->len, FOLL_FORCE);
		if (done > 0 &&
		    copy_to_user((void __user *)(unsigned long)req->buf, kbuf,
				 done)) {
			ret = -EFAULT;
			goto out_buf;
		}
	}

	if (done < 0)
		ret = done;
	req->out = (done < 0) ? 0 : (__u32)done;

out_buf:
	kfree(kbuf);
out_mm:
	mmput(mm);
out_task:
	put_task_struct(task);
	return ret;
}

static long xth_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct xth_req req;
	long ret;

	(void)f;
	if (cmd != XTH_IOC_RW)
		return -ENOTTY;

	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;

	ret = xth_do_rw(&req);

	if (copy_to_user((void __user *)arg, &req, sizeof(req)))
		return -EFAULT;

	return ret;
}

static const struct file_operations xth_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = xth_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = xth_ioctl,
#endif
};

static struct miscdevice xth_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "xth_ko",
	.fops  = &xth_fops,
	.mode  = 0666,
};

static int __init xth_init(void)
{
	int ret = misc_register(&xth_dev);

	if (ret) {
		pr_err("xth_ko: misc_register failed: %d\n", ret);
		return ret;
	}

	pr_info("xth_ko: loaded, /dev/xth_ko ready\n");
	return 0;
}

static void __exit xth_exit(void)
{
	misc_deregister(&xth_dev);
	pr_info("xth_ko: unloaded\n");
}

module_init(xth_init);
module_exit(xth_exit);
