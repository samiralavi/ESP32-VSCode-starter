#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

if [[ -z $IDF_PATH ]]; then
    echo "IDF_PATH is not set. Make sure you are in the correct environment."
    exit 1;
fi

$SCRIPT_DIR/build_www.sh && \
IDF_TOOLCHAIN=clang idf.py -B build_clang_check clang-check -C $PROJECT_ROOT --exclude-paths $PROJECT_ROOT/managed_components \
    --exclude-paths $IDF_PATH --exclude-paths $PROJECT_ROOT/components --exit-code --include-paths $PROJECT_ROOT/main \
    --run-clang-tidy-options -config-file $PROJECT_ROOT/.clang-tidy.yml 
