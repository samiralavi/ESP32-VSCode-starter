#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

if [[ -z $IDF_PATH ]]; then
    echo "IDF_PATH is not set. Make sure you are in the correct environment."
    exit 1;
fi

DIST=$PROJECT_ROOT/dist

rm -rf $DIST \
&& mkdir $DIST \
&& $SCRIPT_DIR/build_www.sh \
&& idf.py -C $PROJECT_ROOT @$PROJECT_ROOT/profiles/dev build \
&& cp $PROJECT_ROOT/build_dev/esp32_starter.bin $DIST \
&& cp $PROJECT_ROOT/build_dev/partition_table/partition-table.bin $DIST \
&& cp $PROJECT_ROOT/build_dev/ota_data_initial.bin $DIST \
&& cp $PROJECT_ROOT/build_dev/bootloader/bootloader.bin $DIST \
&& cp $PROJECT_ROOT/scripts/flash_script.sh $DIST
