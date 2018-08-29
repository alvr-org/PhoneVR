pushd %~dp0\ffmpeg-windows

set dir64="ffmpeg-3.2.2-win64-dev"
set dir32="ffmpeg-3.2.2-win32-dev"

if exist include rmdir /s /q include

if exist %dir64%.zip del /f %dir64%.zip
wget https://ffmpeg.zeranoe.com/builds/win64/dev/%dir64%.zip
7za x -aoa %dir64%.zip
del /f x64\*.lib
xcopy /y %dir64%\lib\*.lib x64
xcopy /eyi %dir64%\include include
del /f %dir64%.zip
rmdir /s /q %dir64%

if exist %dir32%.zip del /f %dir32%.zip
wget https://ffmpeg.zeranoe.com/builds/win32/dev/%dir32%.zip
7za x -aoa %dir32%.zip
del /f x86\*.lib
xcopy /y %dir32%\lib\*.lib x86
del /f %dir32%.zip
rmdir /s /q %dir32%

popd