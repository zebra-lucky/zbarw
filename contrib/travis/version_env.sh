#!/bin/bash

VERSION_STRING=(`grep AC_INIT configure.ac`)
ZBAR_VERSION=${VERSION_STRING[1]}
ZBAR_VERSION=${ZBAR_VERSION:1:-2}
export ZBAR_VERSION