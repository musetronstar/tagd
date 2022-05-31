#!/bin/bash

set -e	# exit on error

DST_DIR=$1

if [ -n $DST_DIR ]; then
	echo "usage: new-app.sh <dest-dir>" 2>&1
	exit 1
fi

[ -d "$DST_DIR" ] || mkdir -p "$DST_DIR"

DST_APP_DIR=${DST_DIR}/app
if [ -d $DST_APP_DIR ]; then
	echo "app already exists: $DST_APP_DIR" 1>&2
	exit 1
else
	mkdir -p $DST_APP_DIR
fi

# this scripts working dir
PRJ_DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")/../" >/dev/null 2>&1 && pwd )"
PRJ_APP_DIR=${PRJ_DIR}/httagd/app
rsync -av --progress ${PRJ_APP_DIR}/* $DST_APP_DIR --exclude '*.o'

cat ${PRJ_APP_DIR}/Makefile \
	| sed "s|PRG_DIR = .*|PRG_DIR = ${PRJ_DIR}|g" > ${DST_APP_DIR}/Makefile
