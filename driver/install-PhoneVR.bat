pushd %~dp0\bin

set drvpath="C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\oculus_PVRServer"
set dir64="ffmpeg-3.2.2-win64-shared"
set dir32="ffmpeg-3.2.2-win32-shared"

xcopy /eyi oculus_PVRServer %drvpath%

if exist %dir64%.zip del /f %dir64%.zip
wget https://ffmpeg.zeranoe.com/builds/win64/shared/%dir64%.zip
7za x -aoa %dir64%.zip
xcopy /y %dir64%\bin\*.dll %drvpath%\bin\win64
del /f %dir64%.zip
rmdir /s /q %dir64%

if exist %dir32%.zip del /f %dir32%.zip
wget https://ffmpeg.zeranoe.com/builds/win32/shared/%dir32%.zip
7za x -aoa %dir32%.zip
xcopy /y %dir32%\bin\*.dll %drvpath%\bin\win32
del /f %dir32%.zip
rmdir /s /q %dir32%

popd

pause