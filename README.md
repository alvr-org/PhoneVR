[![PhonveVR](./.github/LogoPVR.png)](https://github.com/ShootingKing-AM/PhoneVR/releases)<br/>

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
* Copy `driver_PhoneVR.dll` (from `[PhoneVR.zip]/windriver/[win64 or win32]`) to you local-driver-folder)
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

  
