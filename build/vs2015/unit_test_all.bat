@REM
@REM f9omstw\build\vs2015\unit_test_all.bat
@REM

@echo off
set EXEDIR=\devel\output\f9omstw\64\Debug

del *.log

%EXEDIR%\OmsOrdNoMap_UT
%EXEDIR%\OmsRequestPolicy_UT

@REM -----------------------
@REM 測試 2 次, 第2次會載入前次資料.
%EXEDIR%\OmsRequestTrade_UT
%EXEDIR%\OmsRequestTrade_UT

%EXEDIR%\OmsReport_UT -r

@REM -----------------------
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 0

del OmsReqOrd_UT.log
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1
%EXEDIR%\OmsReqOrd_UT -o OmsReqOrd_UT.log -f 1

@REM -----------------------
%EXEDIR%\OmsRcAgents_UT --log=0

@REM -----------------------
%EXEDIR%\OmsRcFramework_UT
%EXEDIR%\OmsRcFramework_UT

@REM -----------------------
del *.f9gv
del *.f9dbf
del *.log
@echo '#####################################################'
@echo '#           f9omstw All tests passed!               #'
@echo '#####################################################'
