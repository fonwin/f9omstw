@echo off

REM 在指定路徑建立 proj_verinfo.h:
REM - Windows 在「Build Events/Pre-Build Event/Command Line」設定:
REM    cd 「proj_verinfo.h 存放路徑」   (例如 cd ..\..\..\YourProjectName)
REM    call ..\f9omstw\make_proj_verinfo.bat
REM - 然後可以在程式裡將版本資放入 SysEnv:
REM   if (auto sysEnv = holder.Root_->Get<fon9::seed::SysEnv>(fon9_kCSTR_SysEnv_DefaultName))
REM      fon9::seed::LogSysEnv(sysEnv->Add(new fon9::seed::SysEnvItem("Version", proj_VERSION)).get());


FOR /F "usebackq tokens=1,2 delims==" %%i IN (`wmic os get LocalDateTime/VALUE 2^>NUL`) DO IF '.%%i.'=='.LocalDateTime.' SET ldt=%%j
SET ldt=%ldt:~0,4%.%ldt:~4,2%.%ldt:~6,2%-%ldt:~8,2%%ldt:~10,2%


pushd %~dp0
FOR /F "tokens=*" %%g IN ('git rev-parse "--short=12" HEAD') DO SET F9OMSTW_HASH=%%g
cd ../fon9
FOR /F "tokens=*" %%g IN ('git rev-parse "--short=12" HEAD') DO SET FON9_HASH=%%g
popd

FOR /F "tokens=*" %%g IN ('git rev-parse "--short=12" HEAD') DO SET MySln_HASH=%%g

SET pwd=%CD%
For %%A in ("%pwd%") do SET PROJ=%%~nxA

echo #ifndef proj_NAME>                                                                                   proj_verinfo.h
echo #define proj_NAME    "%PROJ%">>                                                                      proj_verinfo.h
echo #endif>>                                                                                             proj_verinfo.h
echo #define proj_VERSION proj_NAME "=%MySln_HASH%|fon9=%FON9_HASH%|f9omstw=%F9OMSTW_HASH%|build=%ldt%">> proj_verinfo.h
