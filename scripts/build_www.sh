#!/usr/bin/env bash

FULL_PATH=$(realpath $0)
SCRIPT_DIR=$(dirname $FULL_PATH)
PROJECT_ROOT=$SCRIPT_DIR/..

WWW=$PROJECT_ROOT/main/www
DIST=$WWW/dist

rm -rf $DIST \
&& mkdir $DIST \
&& inliner $WWW/index.html | gzip > $DIST/index.html.gz
