#!/system/bin/sh
# Device-side load / verify / rollback for xth_ko.ko  (run as root)
#
#   su -c 'sh /data/local/tmp/xthko/load.sh [/path/to/xth_ko.ko]'
#
# Rollback:  su -c 'rmmod xth_ko'
set -u

KO="${1:-/data/local/tmp/xthko/xth_ko.ko}"

echo "== modinfo =="
modinfo "$KO" 2>/dev/null | grep -E 'name|license|vermagic|depends' || true

if grep -q '^xth_ko ' /proc/modules 2>/dev/null; then
	echo "[*] already loaded, unloading first"
	rmmod xth_ko || true
fi

echo "== insmod =="
if ! insmod "$KO"; then
	echo "[!] insmod failed"
	dmesg | tail -20
	exit 1
fi

sleep 1

if grep -q '^xth_ko ' /proc/modules; then
	echo "[+] LOAD OK"
	grep '^xth_ko ' /proc/modules
	ls -l /dev/xth_ko
	dmesg | tail -5
	echo "[*] rollback when done: su -c 'rmmod xth_ko'"
else
	echo "[!] module not present after insmod"
	dmesg | tail -20
	exit 1
fi
