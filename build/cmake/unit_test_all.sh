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

OUTPUT_DIR=${OUTPUT_DIR:-${BUILD_DIR}/${BUILD_TYPE}}

rm -f *.log

$OUTPUT_DIR/f9omstw/OmsOrdNoMap_UT
$OUTPUT_DIR/f9omstw/OmsRequestPolicy_UT

# -------------------------------
# 測試 2 次, 第2次會載入前次資料.
$OUTPUT_DIR/f9omstw/OmsRequestTrade_UT
$OUTPUT_DIR/f9omstw/OmsRequestTrade_UT

$OUTPUT_DIR/f9omstw/OmsReport_UT -r
$OUTPUT_DIR/f9omstw/OmsTwfReport_UT -r
$OUTPUT_DIR/f9omstw/OmsTwfReportQuote_UT -r

# -------------------------------
$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o /dev/null -f 0
$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o /dev/null -f 1

$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0
$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0

rm -f OmsReqOrd_UT.log
$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1
$OUTPUT_DIR/f9omstw/OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1

# -------------------------------
$OUTPUT_DIR/f9omsrc/OmsRcAgents_UT --log=0

# -------------------------------
$OUTPUT_DIR/f9omsrc/OmsRcFramework_UT
$OUTPUT_DIR/f9omsrc/OmsRcFramework_UT

# -------------------------------
$OUTPUT_DIR/f9omstw/OmsCx_UT

# -------------------------------
rm -f *.f9gv
rm -f *.f9dbf
{ set +x; } 2>/dev/null
echo '#####################################################'
echo '#           f9omstw All tests passed!               #'
echo '#####################################################'
