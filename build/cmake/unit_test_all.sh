#!/bin/sh
#
# f9omstw/build/cmake/unit_test_all.sh
#

outpath=`pwd`"$0"
outpath="${outpath%/*}"
outpath="${outpath%/*}"
outpath="${outpath%/*}"
develpath="${outpath%/*}"
BUILD_DIR=${BUILD_DIR:-${develpath}/output/f9omstw}
BUILD_TYPE=${BUILD_TYPE:-release}

ulimit -c unlimited

set -x
set -e

OUTPUT_DIR=${OUTPUT_DIR:-${BUILD_DIR}/${BUILD_TYPE}/f9omstw}

$OUTPUT_DIR/OmsRequestPolicy_UT
$OUTPUT_DIR/OmsReqOrd_UT

{ set +x; } 2>/dev/null
echo '#####################################################'
echo '#           f9omstw All tests passed!               #'
echo '#####################################################'
