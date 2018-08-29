pushd %~dp0\bin

set dir32=ffmpeg-3.2.2-win32-shared
set dir64=ffmpeg-3.2.2-win64-shared
set instpath="C:\Program Files\PhoneVR"
mkdir %instpath%
icacls %instpath% /grant:r Everyone:(OI)(CI)F
xcopy /eyi instfiles %instpath%

if exist %dir32%.zip del /f %dir32%.zip
wget https://ffmpeg.zeranoe.com/builds/win32/shared/%dir32%.zip
7za x -aoa %dir32%.zip
xcopy /y %dir32%\bin\*.dll %instpath%\PVRServer\bin\win32
del /f %dir32%.zip
rmdir /s /q %dir32%

if "%PROCESSOR_ARCHITECTURE%" == "x86" (
  "C:\Program Files\Steam\steamapps\common\SteamVR\bin\win32\vrpathreg" adddriver %instpath%\PVRServer
)

if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
  if exist %dir64%.zip del /f %dir64%.zip
  wget https://ffmpeg.zeranoe.com/builds/win64/shared/%dir64%.zip
  7za x -aoa %dir64%.zip
  xcopy /y %dir64%\bin\*.dll %instpath%\PVRServer\bin\win64
  del /f %dir64%.zip
  rmdir /s /q %dir64%
  "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg" adddriver %instpath%\PVRServer
)

popd
pause