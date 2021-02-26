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
F9OMSTW_BUILD_UNIT_TEST=${F9OMSTW_BUILD_UNIT_TEST:-1}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
  && cd $BUILD_DIR/$BUILD_TYPE \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           -DCMAKE_F9OMSTW_BUILD_UNIT_TEST=$F9OMSTW_BUILD_UNIT_TEST \
           $SOURCE_DIR \
  && make $*
