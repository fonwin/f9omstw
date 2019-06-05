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

rm -f *.log

$OUTPUT_DIR/OmsOrdNoMap_UT
$OUTPUT_DIR/OmsRequestPolicy_UT

# 測試 2 次, 第2次會載入前次資料.
$OUTPUT_DIR/OmsRequestTrade_UT
$OUTPUT_DIR/OmsRequestTrade_UT

$OUTPUT_DIR/OmsReqOrd_UT -o /dev/null -f 0
$OUTPUT_DIR/OmsReqOrd_UT -o /dev/null -f 1

$OUTPUT_DIR/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0
$OUTPUT_DIR/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0

rm -f OmsReqOrd_UT.log
$OUTPUT_DIR/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1
$OUTPUT_DIR/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1

{ set +x; } 2>/dev/null
echo '#####################################################'
echo '#           f9omstw All tests passed!               #'
echo '#####################################################'
