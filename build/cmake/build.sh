#!/bin/sh
#
# f9omstw/build/cmake/build.sh
#

srcpath=`pwd`"$0"
srcpath="${srcpath%/*}"
srcpath="${srcpath%/*}"
srcpath="${srcpath%/*}"
develpath="${srcpath%/*}"

set -x

SOURCE_DIR=${srcpath}
BUILD_DIR=${BUILD_DIR:-${develpath}/output/f9omstw}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-${develpath}/${BUILD_TYPE}}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
  && cd $BUILD_DIR/$BUILD_TYPE \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           $SOURCE_DIR \
  && make $*
