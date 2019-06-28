@REM
@REM f9omstw\build\vs2015\unit_test_all.bat
@REM

@echo off
set EXEDIR = \devel\output\f9omstw\64\Debug

del *.log

%EXEDIR%\OmsOrdNoMap_UT
%EXEDIR%\OmsRequestPolicy_UT

@REM -----------------------
@REM test twice.
%EXEDIR%\OmsRequestTrade_UT
%EXEDIR%\OmsRequestTrade_UT

@REM -----------------------
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0

del OmsReqOrd_UT.log
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1

@REM -----------------------
%EXEDIR%\OmsRcServer_UT --log=0

@echo '#####################################################'
@echo '#           f9omstw All tests passed!               #'
@echo '#####################################################'
