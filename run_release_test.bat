@echo off
setlocal

rem Overnight release asset pass: capture every character and stage, then build
rem the review gallery. Usage: run_release_test.bat [asset folder] [gtest filter]

set "ASSET_FOLDER=%~1"
if "%ASSET_FOLDER%"=="" set "ASSET_FOLDER=assets_test"

set "TEST_FILTER=%~2"
if "%TEST_FILTER%"=="" set "TEST_FILTER=MediaCaptureTest.*:CrashTest.StageCrashTest"

if not exist "%ASSET_FOLDER%\data\mugen.cfg" if not exist "%ASSET_FOLDER%\data\dolmexica.cfg" (
	echo No Dolmexica config found under %ASSET_FOLDER%\data - nothing to test.
	exit /b 1
)

if not exist DolmexicaInfiniteTest.exe (
	echo DolmexicaInfiniteTest.exe not found. Build the test project first.
	exit /b 1
)

where python >nul 2>nul && (set "PYTHON=python") || (set "PYTHON=py -3")

set "DOLMEXICA_ASSET_FOLDER=%ASSET_FOLDER%"
echo Testing %ASSET_FOLDER% with filter %TEST_FILTER%

if exist debug\report rmdir /s /q debug\report

echo [%time%] Capturing...
DolmexicaInfiniteTest.exe --gtest_filter=%TEST_FILTER%
if errorlevel 1 echo [%time%] Test run reported failures - the report still covers whatever was captured.

echo [%time%] Building report...
%PYTHON% tools\releasereport\generate_report.py --report-folder debug\report
if errorlevel 1 (
	echo Report generation failed.
	exit /b 1
)

start "" debug\report\report.html
echo [%time%] Done. Review debug\report\report.html, tick what needs a closer look,
echo press "Save manualtest_list.txt", then run run_manual_recheck.bat.
endlocal
