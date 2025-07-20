#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

if [[ -z $IDF_PATH ]]; then
    echo "IDF_PATH is not set. Make sure you are in the correct environment."
    exit 1;
fi

idf.py -C $PROJECT_ROOT flash
