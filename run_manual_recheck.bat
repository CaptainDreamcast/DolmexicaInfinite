@echo off
setlocal enabledelayedexpansion

rem Manual recheck pass over the assets ticked in report.html. Save the list from
rem the report ("Save manualtest_list.txt"), then run this.
rem Usage: run_manual_recheck.bat [asset folder]

set "ASSET_FOLDER=%~1"
if "%ASSET_FOLDER%"=="" set "ASSET_FOLDER=assets_test"

if not exist DolmexicaInfinite.exe (
	echo DolmexicaInfinite.exe not found. Build the game project first.
	exit /b 1
)

if not exist debug mkdir debug

if not exist debug\manualtest_list.txt (
	for /f "delims=" %%F in ('dir /b /o-d "%USERPROFILE%\Downloads\manualtest_list*.txt" 2^>nul') do (
		echo Importing %%F from Downloads.
		copy /y "%USERPROFILE%\Downloads\%%F" debug\manualtest_list.txt >nul
		goto :imported
	)
)
:imported

if not exist debug\manualtest_list.txt (
	echo No debug\manualtest_list.txt. Tick assets in debug\report\report.html,
	echo press "Save manualtest_list.txt", then run this again.
	exit /b 1
)

set /a CHARACTER_AMOUNT=0
set /a STAGE_AMOUNT=0
for /f "usebackq tokens=1 delims=	" %%K in ("debug\manualtest_list.txt") do (
	if /i "%%K"=="character" set /a CHARACTER_AMOUNT+=1
	if /i "%%K"=="stage" set /a STAGE_AMOUNT+=1
)

echo Picked: !CHARACTER_AMOUNT! characters, !STAGE_AMOUNT! stages.
echo Delete debug\manualtest_list.txt to import a newer list from Downloads.
echo.
echo In the game, open the console with ^^ and run:
if not "!CHARACTER_AMOUNT!"=="0" echo     fullcharactertest list
if not "!STAGE_AMOUNT!"=="0" echo     fullstagetest list
echo F1 pass, F7 fail, F8 flag, F9 speed. Verdicts land in debug\manualtest_results.txt.
echo.

set "DOLMEXICA_ASSET_FOLDER=%ASSET_FOLDER%"
DolmexicaInfinite.exe
endlocal
