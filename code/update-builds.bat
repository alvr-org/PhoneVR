pushd %~dp0

xcopy /y mobile\android\PhoneVR\app\build\outputs\apk\app-fat-debug.apk ..\..\Coding\github\PhoneVR\app\PhoneVR.apk

xcopy /eyi "C:\Program Files\PhoneVR\pvrsettings.json" ..\..\Coding\github\PhoneVR\driver\bin
xcopy /eyi "C:\Program Files\PhoneVR\PVRServer\bin\win64\driver_PVRServer.dll" ..\..\Coding\github\PhoneVR\driver\bin\PVRServer\bin\win64
xcopy /eyi "C:\Program Files\PhoneVR\PVRServer\bin\win64\x264.dll" ..\..\Coding\github\PhoneVR\driver\bin\PVRServer\bin\win64
xcopy /eyi "C:\Program Files\PhoneVR\PVRServer\bin\win32\driver_PVRServer.dll" ..\..\Coding\github\PhoneVR\driver\bin\PVRServer\bin\win32
xcopy /eyi "C:\Program Files\PhoneVR\PVRServer\bin\win32\x264.dll" ..\..\Coding\github\PhoneVR\driver\bin\PVRServer\bin\win32

popd

pause