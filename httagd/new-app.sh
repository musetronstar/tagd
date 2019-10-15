#!/bin/bash

set -e	# exit on error

DST_DIR=$1
[ -d $DST_DIR ] || mkdir -p $DST_DIR

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
cp -r ${PRJ_APP_DIR}/* $DST_APP_DIR
find $DST_APP_DIR -name '*.o' -exec rm '{}' \;

cat ${PRJ_APP_DIR}/Makefile \
	| sed "s|PRG_DIR = .*|PRG_DIR = ${PRJ_DIR}|g" > ${DST_APP_DIR}/Makefile
