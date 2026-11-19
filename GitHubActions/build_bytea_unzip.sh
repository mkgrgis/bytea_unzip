#!/bin/bash

################################################################################
#
# This script builds bytea_unzip in PostgreSQL source tree.
#
# Usage: ./build_bytea_unzip.sh pg_version mode sqlite_for_testing_dir
#     pg_version is a PostgreSQL version like 17.0 to be built in.
#     mode is flag for bytea_unzip compiler.
#     sqlite_for_testing_dir: path to install directory of SQLite version for testing
#
# Requirements
# - the source code of bytea_unzip is available by git clone.
# - the source code of PostgreSQL is located in ~/workdir/postgresql-{pg_version}.
# - unzip C library development package is installed in a system.
################################################################################

VERSION="$1"
MODE="$2"

mkdir -p ./workdir/postgresql-${VERSION}/contrib/bytea_unzip
tar zxf ./bytea_unzip.tar.gz -C ./workdir/postgresql-${VERSION}/contrib/bytea_unzip/
cd ./workdir/postgresql-${VERSION}/contrib/bytea_unzip

# show locally compiled sqlite library
ls -la /usr/local/lib

make

sudo make install
