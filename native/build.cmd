@echo off
rem Builds ReadEye_Native.exe using the ScopeCppSDK toolchain bundled with Visual Studio.
rem No C++ workload install required. Falls back through known VS editions.
setlocal

set SDKROOT=
for %%V in ("18\Insiders" "2022\Preview" "2022\Community" "2022\Professional" "2022\Enterprise" "2022\BuildTools") do (
    if exist "C:\Program Files\Microsoft Visual Studio\%%~V\SDK\ScopeCppSDK\vc15\VC\bin\cl.exe" (
        set "SDKROOT=C:\Program Files\Microsoft Visual Studio\%%~V\SDK\ScopeCppSDK\vc15"
        goto :found
    )
)
echo ERROR: ScopeCppSDK cl.exe not found. Install Visual Studio or adjust SDKROOT.
exit /b 1

:found
echo Using toolchain: %SDKROOT%
set "INCLUDE=%SDKROOT%\VC\include;%SDKROOT%\SDK\include\ucrt;%SDKROOT%\SDK\include\um;%SDKROOT%\SDK\include\shared"
set "LIB=%SDKROOT%\VC\lib;%SDKROOT%\SDK\lib"

"%SDKROOT%\VC\bin\cl.exe" /nologo /O2 /W4 /EHsc /DUNICODE /D_UNICODE ^
    "%~dp0main.cpp" /Fe:"%~dp0..\ReadEye_Native.exe" /Fo:"%~dp0main.obj" ^
    /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib advapi32.lib gdiplus.lib

set BUILD_RESULT=%ERRORLEVEL%
if exist "%~dp0main.obj" del "%~dp0main.obj"
exit /b %BUILD_RESULT%
