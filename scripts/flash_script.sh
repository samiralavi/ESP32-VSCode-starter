#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

python -m esptool --chip esp32s3 -b 460800 --before default_reset \
 --after hard_reset write_flash --flash_mode dio --flash_size 8MB \
 --flash_freq 80m 0x0 $SCRIPT_DIR/bootloader.bin 0x8000 \
 $SCRIPT_DIR/partition-table.bin 0x9000 $SCRIPT_DIR/ota_data_initial.bin \
 0xa00000 $SCRIPT_DIR/esp32_starter.bin
