<br/><br/><br/>
[![PhonveVR](./.github/LogoPVR.png)](https://github.com/ShootingKing-AM/PhoneVR/releases)<br/>
<br/><br/><br/>

<img src="./.github/rep1.png" width="50%"><img src="./.github/rep2.jpg" width="50%">
<sup>Pictures used for representational purposes</sup>

[![Build status](https://ci.appveyor.com/api/projects/status/1eyjmo51o4c86r07?svg=true)](https://ci.appveyor.com/project/ShootingKing-AM/phonevr)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/ShootingKing-AM/PhoneVR?color=orange&include_prereleases)

Use Steam VR-enabled applications with your phone as HMD (*Head-mounted display*). The only Open-Source solution to similar commercial packages like VRidge, Riftcat, Trinus etc etc.
<br/>

## Requirements

A PC with *Windows 7 or above*, A smartphone with *Android 5.0(Lollipop) or above* with *OpenGL-ES 3.0 or above*, Steam and some SteamVR games installed.

## Installation

* Make sure you have Steam and SteamVR installed (To find SteamVR on steam, `Library -> Tools -> SteamVR`).
* Download latest [`PhoneVR.zip`](https://github.com/ShootingKing-AM/PhoneVR/releases) release of this repository.
* Download the repository files. Run the script `install-PhoneVR.bat`, located in the `driver` folder.
  - **Note**: The batch file assumes that "Steam" is installed in Default path, if have installed Steam in a different path you might have to have edit the batch file `Ln39 (Win32)` or `Ln41 (Win64)` respecitively. <br/>
    *Eg*. If you have installed Steam in different driver altogether, now, If your Steam Location is `I:\Program Files\Steam` and you are on `Win64`,
        you would change the `Ln41` from <br/>
        `  "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrpathreg" adddriver %instpath%\PVRServer` to <br/>
        `  "I:\Program Files\Steam\steamapps\common\SteamVR\bin\win32\vrpathreg" adddriver %instpath%\PVRServer`
* Copy `driver_PVRServer.dll` (from `[PhoneVR.zip]/windriver/[win64 or win32]`) to you local-driver-folder)
  `local-driver-folder` by default is: `C:\Program Files\PhoneVR\PVRServer\bin\[win32 or win64]\`
* Install the Apk on your mobile from `[PhoneVR.zip]/android/arm64`.

To **play**, **first open the Phone App**(SteamVR should also be closed), then run the game of your choice on PC. (Obviously, both PC and Mobile should be on same Network, preferably Wifi 5.0)

## Development
This Project is presently under testing. But, pull requests are welcome. 

* Windows Driver: `<root>/code/windows/PhoneVR.sln`
  * Compiled/Tested on `Microsoft Visual Studio 2017`
  * After building copy the `driver_PhoveVR.dll` and other files from `<solution root>/build/[win32 or x64]/` to local driver folder.
  * For Runtime debugging, You need to Attach `MSVS JIT debugger` to `vrserver.exe` (which actually loads the driver_PhonveVR.dll)
  * For testing, this project has 2 Build Configs, Debug and Release. Debug has lots of debugging callouts to both local-driver-folder/pvrlog.txt and MSVS Debugger Output.

* Android App: App folder: `<root>/code/mobile/android/PhoneVR`
  * Compiled/Tested on `Android Studio 4.0.1`
  * For testing, this project has 2 Build Configs, Debug and Release. Debug has lots of debugging callouts to logcat from both JAVA and JNI.
  
* External Vendor Libraries used (all Headers included in respective Projects):
  * Json v3.8.0 (https://github.com/nlohmann/json) (code\windows\libs\json)
  * Eigen v3.3.7 (https://gitlab.com/libeigen/eigen) (code\common\libs\eigen)
  * Asio v1.12.2 (https://sourceforge.net/projects/asio/files/asio/1.12.2%20%28Stable%29/) (code\common\libs\asio)
  * x264 0.161.r3015 MSVS15(2017) (https://github.com/ShiftMediaProject/x264)
  * GoogleVR 1.200 (https://github.com/googlevr/gvr-android-sdk) (code\mobile\android\libraries\jni)

## Troubleshooting  
* Android App doesn’t connect to Windows Steam VR even after opening Phone App first and then SteamVR on windows
  1. Make sure that both the Windows and Android devices are in the same Local Network. (connected to the same router/wifi device)
  2. Sometimes, the port which PhoneVR use to connect win/mobile devices, also known as `Pairing Port (default :33333)`, might be used by other services on your devices(Windows/Android). Try changing the "Pairing Port" on `Android PhoneVR App` settings and `pairing_port` in `C:\Program Files\PhoneVR\pvrsettings.json` for `Windows PVR driver` and restart SteamVR. Both Windows Pairing port and Android Pairing port needs to be same. Safe recommended port range : `30000 - 65535`
  
* Android App automatically comes back to "Discovery"(Home/AppStart) page after some VR Application usage
  - Check if `Android System Battery saver` or similar applications are killing the app when in background. Usually can be found is `Android Setting` -> `Application Manager` or `Application Settings` according to your Android device flavour/OEM.

## Issues
You can use the `Github Issues` to sumbit any issues related to working of this Solution.
For quick resolution you may want to add the following data along with your issue,
* `steamvr.vrsettings` file in default location `C:\Program Files (x86)\Steam\config\steamvr.vrsettings`
* `vrserver.txt` file in default location `C:\Program Files (x86)\Steam\logs\vrserver.txt`
* `pvrLog.txt` and/or `pvrDebugLog.txt` file(s) in default location `C:\Program Files\PhoneVR`
* Open a `cmd` in the follow default directory and copy paste output of the `vrcmd` command. `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrccmd.exe`
* And ofcourse, how to reproduce the issue :)

