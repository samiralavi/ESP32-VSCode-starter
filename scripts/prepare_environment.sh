#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

source $IDF_PATH/export.sh
# install clang-tidy
idf_tools.py install esp-clang
# reload the environment after installation
source $IDF_PATH/export.sh
