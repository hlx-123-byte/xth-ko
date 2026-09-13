#!/usr/bin/env bash
# Build xth_ko.ko against a prepared arm64 5.10 kernel tree.
#
# Build host requirement: a Linux environment (WSL2/Ubuntu, a VM, or the
# KernelSU "ddk" container). Windows cannot build kernel modules natively.
#
# Prereqs (Ubuntu/WSL):
#   sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
#        clang lld llvm make
#
# Usage:
#   KDIR=/path/to/kernel-source ./build.sh
#   KDIR=/path/to/kernel-source DISABLE_MODVERSIONS=1 ./build.sh
#
# Notes:
#   - This kernel bypasses vermagic/modversions, so the tree does NOT have to
#     match the phone exactly. Any 5.10 GKI/vendor arm64 tree works as long as
#     the symbols we import stay exported (they are all EXPORT_SYMBOL/GPL).
#   - Default keeps CONFIG_MODVERSIONS disabled so the module gets an empty
#     __versions section (same shape as the on-device pathmask.ko that loads).
set -euo pipefail

KDIR="${KDIR:?set KDIR to a prepared arm64 kernel source/build dir}"
JOBS="${JOBS:-$(command -v nproc >/dev/null && nproc || echo 4)}"
HERE="$(cd "$(dirname "$0")" && pwd)"

echo "[*] KDIR=$KDIR  jobs=$JOBS"

if [ "${DISABLE_MODVERSIONS:-1}" = "1" ] && [ -x "$KDIR/scripts/config" ]; then
	echo "[*] disabling CONFIG_MODVERSIONS in tree config"
	"$KDIR/scripts/config" --file "$KDIR/.config" -d MODVERSIONS || true
	make -C "$KDIR" ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig >/dev/null
fi

echo "[*] building module"
make -C "$KDIR" M="$HERE" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
	KBUILD_MODPOST_WARN=1 -j"$JOBS" modules

echo "[*] done"
ls -l "$HERE/xth_ko.ko"
"${LLVM_READELF:-llvm-readelf}" -SW "$HERE/xth_ko.ko" 2>/dev/null | grep -E '__versions|\.modinfo' || true
"${LLVM_READELF:-llvm-readelf}" -p .modinfo "$HERE/xth_ko.ko" 2>/dev/null | grep vermagic || true
