@ECHO OFF

xcopy "C:\Users\narni\Documents\GitHub\PhoneVR\code\windows\PhoneVR\Debug\x64" "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\PVRServer\bin\win64" /h /i /c /k /e /r /y

::CALL explorer "steam://rungameid/250820"

:::LOOP
::tasklist | find /i "vrserver" >nul 2>&1
::IF ERRORLEVEL 1 (
::  ECHO vrserver is still starting
::  GOTO LOOP
::) ELSE (
::  ECHO vrserver started
::  GOTO CONTINUE
::)

:CONTINUE

exit 0