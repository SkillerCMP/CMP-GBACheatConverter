@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
    echo Drag one or more .cht files onto this BAT file.
    echo.
    pause
    exit /b 1
)

set "EXE=%~dp0GbaCheatConverterCLI.exe"
if not exist "%EXE%" set "EXE=%~dp0GbaCheatConverter.exe"
if not exist "%EXE%" (
    echo ERROR: GbaCheatConverterCLI.exe or GbaCheatConverter.exe
    echo was not found in the same folder as this BAT file.
    echo.
    pause
    exit /b 1
)

set "FAILED=0"

:convert_next
if "%~1"=="" goto :finished

if not exist "%~f1" (
    echo ERROR: Input file was not found:
    echo "%~f1"
    echo.
    set "FAILED=1"
    shift
    goto :convert_next
)

set "INPUT=%~f1"
set "OUTPUT=%~dpn1.clt"

echo Converting:
echo   "!INPUT!"
"%EXE%" --from auto --to vba-clt --output "!OUTPUT!" "!INPUT!"
if errorlevel 1 (
    echo ERROR: Conversion failed.
    echo.
    set "FAILED=1"
    shift
    goto :convert_next
)

if not exist "!OUTPUT!" (
    echo ERROR: The output file was not created.
    echo.
    set "FAILED=1"
    shift
    goto :convert_next
)

for %%F in ("!OUTPUT!") do set "OUTPUT_SIZE=%%~zF"
if "!OUTPUT_SIZE!"=="0" (
    echo ERROR: The output file is empty.
    del "!OUTPUT!" >nul 2>&1
    echo.
    set "FAILED=1"
    shift
    goto :convert_next
)

echo Created:
echo   "!OUTPUT!" ^(!OUTPUT_SIZE! bytes^)
echo.
shift
goto :convert_next

:finished
if "%FAILED%"=="0" (
    echo All conversions completed successfully.
) else (
    echo One or more conversions failed.
)
echo.
pause
exit /b %FAILED%
