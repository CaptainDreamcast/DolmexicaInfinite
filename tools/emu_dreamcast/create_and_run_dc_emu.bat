REM create build folder
cd ..\..
rmdir /s /q build_dreamcast
mkdir build_dreamcast

REM copy over make your own game kit
xcopy /s /e /i /y "tools\release\dreamcast\" "build_dreamcast"

REM copy over assets
mkdir "build_dreamcast\filesystem"
mkdir "build_dreamcast\filesystem\assets"
mkdir "build_dreamcast\ip"
xcopy /s /e /i /y assets "build_dreamcast\filesystem\assets"
xcopy /i /y boot\INSERT.png "build_dreamcast\ip\INSERT.png"*
xcopy /i /y boot\ip.txt "build_dreamcast\ip\ip.txt"*
tools\release\build_tools\dreamcast\scramble.exe 1ST_READ.BIN "build_dreamcast\filesystem\1ST_READ.BIN"

REM create cdi
cd "build_dreamcast"
call make_cdi.bat TEST test

REM run cdi
C:\DEV\PLATFORMS\DREAMCAST\EMU2\flycast.exe test.cdi

REM cleanup
cd ..\tools\emu_dreamcast