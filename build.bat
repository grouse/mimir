@ECHO off

set start_time=%time%

SET ROOT=%~dp0
SET BUILD_DIR=%ROOT%\build

SET DEFINITIONS=-D_CRT_SECURE_NO_WARNINGS

SET WARNINGS=-Wall -Wextra -Wno-missing-braces -Wno-logical-op-parentheses -Wno-c99-designator -Wno-unused-parameter -Wno-initializer-overrides -Wno-c++20-extensions
SET FLAGS=%DEFINITIONS% %WARNINGS% -ferror-limit=0 -g -fno-exceptions -fno-rtti -std=c++17 -fno-color-diagnostics

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

SET INCLUDE_DIR=-I%ROOT%
SET LIBS=-luser32.lib -lShell32.lib -lopengl32.lib -lgdi32.lib -lshlwapi.lib
SET LLVM=D:\apps\LLVM

PUSHD %BUILD_DIR%
echo compiling...
clang++ -O0 %FLAGS% %INCLUDE_DIR% %LIBS% -o mimir.exe %ROOT%\src\win32_mimir.cpp
POPD

set end_time=%time%

set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%start_time%") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%end_time%") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100

set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 100%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%
if 1%ms% lss 100 set ms=0%ms%

set /a totalsecs = %hours%*3600 + %mins%*60 + %secs%
echo finished in %totalsecs%.%ms% seconds
