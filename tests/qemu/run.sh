#!/usr/bin/env bash
# Corre o teste no QEMU do ESP32-S3 (sem placa). Requer o ESP-IDF 5.4 activo e o QEMU:
#   python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
set -euo pipefail
cd "$(dirname "$0")"
idf.py set-target esp32s3 >/dev/null && idf.py build
cd build && esptool.py --chip esp32s3 merge_bin --fill-flash-size 4MB -o flash.bin @flash_args
timeout 600 qemu-system-xtensa -nographic -machine esp32s3 \
    -drive file=flash.bin,if=mtd,format=raw | grep -a --line-buffered -E "TESTE|espeak|Guru" | \
    { while read -r l; do echo "$l"; case "$l" in *"TESTE TUDO OK"*|*"COM FALHAS"*|*Guru*) pkill -f qemu-system-xtensa; break;; esac; done; }
