// xth_client - userspace test client for /dev/xth_ko
//
// Build (NDK, from Windows):
//   pwsh -File kernel/build-client.ps1
//
// Run on device (root):
//   adb push xth_client /data/local/tmp/xthko/
//   adb shell su -c '/data/local/tmp/xthko/xth_client dump <pid> <hexaddr> <len>'
//   adb shell su -c '/data/local/tmp/xthko/xth_client write <pid> <hexaddr> <hexbytes>'
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define XTH_MAGIC 0x58

struct xth_req {
	uint32_t pid;
	uint32_t op;
	uint64_t addr;
	uint64_t buf;
	uint32_t len;
	uint32_t out;
};

#define XTH_IOC_RW _IOWR(XTH_MAGIC, 0x01, struct xth_req)
#define XTH_DEV "/dev/xth_ko"

static int xth_rw(int fd, uint32_t pid, uint32_t op, uint64_t addr,
		  void *buf, uint32_t len, uint32_t *out)
{
	struct xth_req req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.pid = pid;
	req.op = op;
	req.addr = addr;
	req.buf = (uint64_t)(uintptr_t)buf;
	req.len = len;

	ret = ioctl(fd, XTH_IOC_RW, &req);
	if (out)
		*out = req.out;
	return ret;
}

static int hex2bin(const char *hex, unsigned char *out, size_t outsz)
{
	size_t i, n = strlen(hex) / 2;

	if (n > outsz || (strlen(hex) & 1))
		return -1;
	for (i = 0; i < n; i++) {
		unsigned int b;
		if (sscanf(hex + 2 * i, "%2x", &b) != 1)
			return -1;
		out[i] = (unsigned char)b;
	}
	return (int)n;
}

int main(int argc, char **argv)
{
	int fd;
	uint32_t pid, len, out = 0;
	uint64_t addr;

	if (argc < 5) {
		fprintf(stderr,
			"usage:\n"
			"  %s dump  <pid> <hexaddr> <len>\n"
			"  %s write <pid> <hexaddr> <hexbytes>\n",
			argv[0], argv[0]);
		return 2;
	}

	pid = (uint32_t)strtoul(argv[2], NULL, 10);
	addr = strtoull(argv[3], NULL, 16);

	fd = open(XTH_DEV, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", XTH_DEV, strerror(errno));
		return 1;
	}

	if (!strcmp(argv[1], "dump")) {
		unsigned char *buf;
		uint32_t i;

		len = (uint32_t)strtoul(argv[4], NULL, 10);
		if (len == 0 || len > (16u * 1024u * 1024u)) {
			fprintf(stderr, "bad len\n");
			close(fd);
			return 1;
		}
		buf = malloc(len);
		if (!buf) {
			close(fd);
			return 1;
		}
		if (xth_rw(fd, pid, 0, addr, buf, len, &out) < 0) {
			fprintf(stderr, "ioctl: %s\n", strerror(errno));
			free(buf);
			close(fd);
			return 1;
		}
		printf("read %u bytes from pid %u @ 0x%llx\n", out, pid,
		       (unsigned long long)addr);
		for (i = 0; i < out; i++) {
			printf("%02x", buf[i]);
			if ((i + 1) % 16 == 0)
				printf("\n");
			else
				printf(" ");
		}
		if (out % 16)
			printf("\n");
		free(buf);
	} else if (!strcmp(argv[1], "write")) {
		static unsigned char buf[16 * 1024 * 1024];
		int n = hex2bin(argv[4], buf, sizeof(buf));

		if (n < 0) {
			fprintf(stderr, "bad hex\n");
			close(fd);
			return 1;
		}
		if (xth_rw(fd, pid, 1, addr, buf, (uint32_t)n, &out) < 0) {
			fprintf(stderr, "ioctl: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		printf("wrote %u bytes to pid %u @ 0x%llx\n", out, pid,
		       (unsigned long long)addr);
	} else {
		fprintf(stderr, "unknown op %s\n", argv[1]);
		close(fd);
		return 2;
	}

	close(fd);
	return 0;
}
