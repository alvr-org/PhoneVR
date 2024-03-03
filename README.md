<br/><br/>
[![PhonveVR](./.github/LogoPVR.png)](https://github.com/PhoneVR-Developers/PhoneVR/releases)<br/>
![LogoPVRv0 3](https://github.com/PhoneVR-Developers/PhoneVR/assets/4137788/b32a23f6-4264-45f9-99cc-bbdd593a8e94)

<br/>

<img src="./.github/rep1.png" width="50%"><img src="./.github/rep2.jpg" width="50%">
<sup>Pictures used for representational purposes</sup>

[![Build status](https://github.com/PhoneVR-Developers/PhoneVR/actions/workflows/build.yml/badge.svg)](https://github.com/PhoneVR-Developers/PhoneVR/actions)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/PhoneVR-Developers/PhoneVR?color=orange&include_prereleases)
[![Discord](https://img.shields.io/discord/745688786410930296?label=Discuss&logo=Discord&logoColor=white)](https://discord.gg/pNYtBNk)

Use Steam VR-enabled applications with your phone as VR HMD (*Head-mounted display*). The only Open-Source solution to similar commercial packages like VRidge, Riftcat, Trinus etc etc. *Common-network* can be any type of network between desktop and android app, even **USB Tethering** and **Mobile Hotspots** are supported.
<br/>

| Tabs | Links |
| --- | --- |
| Roadmap/Plan/Future Todo | [**Project Kanban**](https://github.com/orgs/PhoneVR-Developers/projects/1/views/1) | 
| Latest Release | [`Github Release`](https://github.com/PhoneVR-Developers/PhoneVR/releases/latest) |
| Previous Builds | [`Github Actions`](https://github.com/PhoneVR-Developers/PhoneVR/actions) |
| Issues | [`Github Issue Tracker`](https://github.com/PhoneVR-Developers/PhoneVR/issues) |
| Discuss/Chat/Help/Real-Time Updates/Any - Discord | [`Discord Server Invite Link`](https://discord.com/invite/pNYtBNk) |

## Contents
* [Requirements](#requirements)
* [Installation](#installation)
* [Advanced Configuration](#advanced-configuration)
* [Development](#development)
* [Troubleshooting](#troubleshooting)
* [Issue / Bug Reporting](#issue--bug-reporting)

## Requirements

A PC with *Windows 7 or above*, A smartphone with *Android 5.0 (Lollipop) or above* (_ALVR requires Android 8.0 or above_) with *OpenGL-ES 3.0 or above*, Steam and some SteamVR applications installed.

## Installation
PhoneVR(PVR) can be used with two servers ALVR and PVRServer. ALVR Server is recommended over PVRServer, both use different APIs.

| Differences                                                                    | ALVR Server                                  | PhoneVR Server             |
| ------------------------------------------------------------------------------ | -------------------------------------------- | -------------------------- |
| Server/Desktop Side                                                            | OpenVR API                                   | OpenVR API                 |
| Mobile Side                                                                    | Google Cardboard API                         | Google VR API (deprecated) |
| GUI                                                                            | ✅                                           | ❌                         |
| User friendly Installer                                                        | ✅                                           | ❌                         |
| Serverside hardware(GPU) Encoding                                              | ✅                                           | ❌                         |
| Serverside software Encoding                                                   | ✅                                           | ✅                         |
| Automatic Firewall setting                                                     | ✅                                           | ❌                         |
| Linux Support (\*Partial\*)                                                    | ✅                                           | ❌                         |
| Developers                                                                     | [Alvr-org](https://github.com/alvr-org/ALVR) | PhoneVR                    |
| Supports other HMDs ? Quest etc.                                               | ✅                                           | ❌                         |
| Open-Source                                                                    | ✅                                           | ✅                         |
| Network throttling                                                             | ✅                                           | ❌                         |
| Average Latency                                                                | 150ms                                        | 1500ms                     |
| VRApplication Graphics manipulation<br>(Brightness, resolution, contrast etc.) | ✅                                           | (Partial)                  |
| Code backend                                                                   | Rust + C++                                   | C++                        |

### A. Installation using ALVR Server
* Currently only **[ALVR v20.6.1](https://github.com/alvr-org/ALVR/releases/tag/v20.6.1)** (latest stable) is supported by PhoneVR.
  * Older version
    - PhoneVR v1.2.0-beta -> ALVR v20.5.0
    - PhoneVR v1.1.0-beta -> ALVR v20.2.1
    - PhoneVR v1.0.0-beta -> ALVR v20.1.0
* Use the install guide of ALVR Server from [here](https://github.com/alvr-org/ALVR#install).

### B. Installation using Legacy PhoneVR Server
* Make sure you have Steam and SteamVR installed (To find SteamVR on steam, `Library -> Tools -> SteamVR`).
* Download latest release [`PhoneVR-Server-vX.X.X-xxxxx.zip`](https://github.com/PhoneVR-Developers/PhoneVR/releases/latest) of this repository.
* Copy the whole folder `PVRServer` in `driver` folder of zip file into your `SteamVR/drivers` folder. (Default Path: `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers`.
* (Only for PhoneVR Server) To **play**, **first open the Phone App**(SteamVR should also be closed), then run the game of your choice on PC. (Obviously, both PC and Mobile should be on same Network, preferably Wifi 5.0)

### Common Instructions for both Servers
* Install the Android Apk on your mobile from [`Releases`](https://github.com/PhoneVR-Developers/PhoneVR/releases/latest) page.
* Make sure that "Run in Background", "Auto Start"(Restart on Crash) permissions, if exists on your device(especially Xiaomi users), are given. Also make sure that any kind of 3rd party battery saver app dosen't kill PhoneVR when in background.
* After opening the installed app, Choose ALVR(recommended) or PhoneVR Server, according to the server you installed.

## Advanced Configuration 

### for ALVR Server
* This can be found [here](https://github.com/alvr-org/ALVR/wiki/Settings-guide) and [here](https://github.com/alvr-org/ALVR/wiki/Settings-tutorial).

### for Phone VR Server
* **Windows Driver Settings**

  Windows SteamVR Driver, auto loads with SteamVR, and gets `FrameTextures` from VRApplication(like SteamVR Home, VRChat, etc.) via SteamVR. These Textures are then Encoded and Streamed, at specified `game_fps`, to Mobile device using x264 Encoder. Some configurations of this encoder can be adjusted via, `pvrsettings.json` in default installation location `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\PVRServer\`.<br/>
  **Default Settings :**
    ```json
    {
        "enable" : true,
        "game_fps" : 65,
        "video_stream_port" : 15243,
        "pose_stream_port" : 51423,
        "pairing_port" : 33333,
        "encoder" : {
            "preset" : "ultrafast",
            "tune" : "zerolatency",
            "qp" : 20,
            "profile" : "baseline"
        }
    }
    ```
    Most settings are self-explanatory. Any change in settings will require SteamVR to be restarted for application.
    * *"encoder"* : Encoder x264 Settings
      - *"preset"* : Can be set to "ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow" or "placebo"
        Warning: the speed of these presets scales dramatically.  Ultrafast is a full 100 times faster than placebo!
        These presets affect the encoding speed. Using a slower preset gives you better compression, or quality per filesize, whereas faster presets give you worse compression. In general, you should just use the preset you can afford to wait for. 
        
      - *"tune"* : Can be set to "film", "animation", "grain", "stillimage", "psnr", "ssim", "fastdecode", "zerolatency"
      Tune affects the video quality(and size), film being the best quality and gradually decreasing to the right
      
      - *"profile"* :  Can be set to  "baseline", "main", "high", "high10", "high422", "high444"
      Applies the restrictions of the given profile. Currently available profiles are specified above, from most to least restrictive. Does NOT guarantee that the given profile will be used: if the restrictions of "High" are applied to settings that are already Baseline-compatible, the stream will remain baseline.  In short, it does not increase settings, only decrease them.
      
      - *"qp"* : Quantization packet constant, Range 0-51, 0-Lossless 
      
      - *"rc_method"* : Rate Control Method, 1/2/3
      Constant QP (CQP) - 0; 
      Constant Rate factor (CRF) - 1; 
      Average Bitrate (ABR) - 2;
      Default is 1 i.e., CRF with rf 24 (and rf_max 26)
      
      Other standard x264 settings,
      - *"qcomp"* : Default : 0
      - *"keyint_max"* : Default : -1
      - *"intra_refresh"* : Default : False
      - *"bitrate"* : Default : -1 (min 1000)
      
  Do only adjust when required, and keep a eye on `pvrlog.txt` file in default installation location `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\PVRServer\logs\` for "*Skipped frame! Please re-tune the encoder parameters*" messages. If you are getting these messages in excess, you may wish to downgrade settings since windows does not seems to have enough processing power and resources to render with existing settings.

## Development
This Project is presently under testing. But, pull requests are welcome. 

* Windows PhoneVR Driver: `<root>/code/windows/PhoneVR.sln`
  * Compiled/Tested on `Microsoft Visual Studio 2017`
  * After building copy the `driver_PhoveVR.dll` and other files from `<solution root>/build/[win32 or x64]/` to local driver folder.
  * For Runtime debugging, You need to Attach `MSVS JIT debugger` to `vrserver.exe` (which actually loads the driver_PhonveVR.dll)
  * For testing, this project has 2 Build Configs, Debug and Release. Debug has lots of debugging callouts to both local-driver-folder/pvrlog.txt and MSVS Debugger Output.

* Android App: App folder: `<root>/code/mobile/android/PhoneVR`
  * Compiled/Tested on `Android Studio 4.0.1`
  * For testing, this project has 2 Build Configs, Debug and Release. Debug has lots of debugging callouts to logcat from both JAVA and JNI.
  * PhoneVR is Android AVD compatible, though PhoneVR Server networking is also AVD compatible, but ALVR Server networking need some more work for simulation.
    * PhoneVR APK when run on AVD automatically assigns 10.0.2.2 (default emu addr) to pcIP
    * After starting the AVD Emulator, need to run "adb forward tcp:33333 tcp:33333", in terminal/cmd, this will enable PVRServer -> Android Emulator based PVRAndroidApp communication.
  
* External Vendor Libraries used (all Headers included in respective Projects):
  * Json v3.8.0 (https://github.com/nlohmann/json) (code\windows\libs\json)
  * Eigen v3.3.7 (https://gitlab.com/libeigen/eigen) (code\common\libs\eigen)
  * Asio v1.12.2 (https://sourceforge.net/projects/asio/files/asio/1.12.2%20%28Stable%29/) (code\common\libs\asio)
  * x264 0.161.r3015 MSVS15(2017) (https://github.com/ShiftMediaProject/x264)
  * GoogleVR 1.200 (https://github.com/googlevr/gvr-android-sdk) (code\mobile\android\libraries\jni)
  * ACRA v5.7 (https://github.com/ACRA/acra)
  * ALVR (https://github.com/alvr-org/ALVR) (code\mobile\android\PhoneVR\ALVR)

* Code styling
  * Repo enforces clang-format v16.0.0 (.clang-format) for C/C++ and spotless (.editorconfig, google-java-format, ktfmt) for Java/Kotlin files
  * To autofix clang-format'ing issues devs can use `make format`
  * To autofix spotless format issues devs can goto PhoneVR android project folder and use `./gradlew spotlessApply` or use [spotless-gradle](https://plugins.jetbrains.com/plugin/18321-spotless-gradle) plugin for AndroidStudio/IDEs

## Troubleshooting  
* Android App doesn’t connect to Windows Steam VR even after opening Phone App first and then SteamVR on windows
  1. Make sure that both the Windows and Android devices are in the same Local Network. (connected to the same router/wifi device)
  2. Sometimes, the port which PhoneVR use to connect win/mobile devices, also known as `Pairing Port (default :33333)`, might be used by other services on your devices(Windows/Android). Try changing the "Pairing Port" on `Android PhoneVR App` settings and `pairing_port` in `C:\Program Files\PhoneVR\pvrsettings.json` for `Windows PVR driver` and restart SteamVR. **Both Windows Pairing port and Android Pairing port should be the same**. Safe recommended port range : `30000 - 65535`
  
* Android App automatically comes back to "Discovery"(Home/AppStart) page after some VR Application usage
  - Check if `Android System Battery saver` or similar applications are killing the app when in background. Usually can be found is `Android Setting` -> `Application Manager` or `Application Settings` according to your Android device flavour/OEM.

* Lag while using VR App on mobile
  - Make sure you are not using debug variant of APK(unless you really want to).
  - All relevant component's FPS are displayed on Mobile device when `Settings -> Debug` is checked. You can find out what the bottleneck component in the whole chain from VRApplication on windows to GoogleVR SDK renderer on Android is and resolve it.
  
* Incase above mentioned things have not been helpful to you, then you might want to **Install Debug variant of APK** from `[PhoneVR.zip]/android/arm7/debug` which can help you/us to get more detailed logs.
  
* **Diagnostics Stats Nomenclature (ONLY PhoneVR Server)** (in order from VRAppFrameCreation to MobileFrameDisplay):<br/>
  These stats can be enabled by checking `Settings`->`Stats` option. All the values must be stable and more or less equal to `game_fps`± 10 value in `pvrsettings.json`.
  <details><summary>Nomenclature</summary>

  ```css
  --- @M - Mobile; @C - CPU/Desktop ---
  ```
  ```css
  @FPS(s) :
  SR - StreamReceiver @M
  D - Media Decoder @M
  R - Frame Renderer @M
  
  cR - Frame Renderer @C
  SS - Stream Sender @C
  SW - Stream Writer @C
  E - Media Encoder @C
  VRa - VR Application FPS @C
  
  @Latency(s) :
  tSS - Time Delay between RendererGotFrame and RendererRendered @C
  tE - Time Delay between EncoderGotFrame and EncoderEncoded @C
  tND - NetworkDelay - Time Delay between DataPacketSent from @C to DataPacketReceived @M
  tSR - StreamReceiver - Time Delay between DataPacketSent from @C to DataSentToMediaDecoder @M
  ```

  ![PhonveVR FPS](./.github/fps.jpg)  
  </details>

## Issue / Bug Reporting

Prior to reporting your Issue/Bug, please check out the ongoing issues ([here](https://github.com/PhoneVR-Developers/PhoneVR/issues) and [here](https://github.com/alvr-org/ALVR/issues)), If you have the same issue, you can join and watch that discussion(s).

You can use the [`Github Issues`](https://github.com/PhoneVR-Developers/PhoneVR/issues) to submit PhoneVR related issues/bugs related to working of this Project or for any query.
For reporting ALVRServer related issues please use [`ALVR Github Issues`](https://github.com/alvr-org/ALVR/issues).

For quick resolution you may want to add the following data along with your issue/bug report,

<ins>For Installation-time or SteamVR-and-PhoneVR-linking issues,</ins>
* `steamvr.vrsettings` file in default location `C:\Program Files (x86)\Steam\config\steamvr.vrsettings`
* `vrserver.txt` file in default location `C:\Program Files (x86)\Steam\logs\vrserver.txt`
* Open a `cmd` in the follow default directory and copy paste output of the `vrcmd` command. `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrcmd.exe`

<ins>For networking issues,</ins>

* Network Capture of Desktop `.pcap` file. How-to-get-it-> [Here](https://github.com/PhoneVR-Developers/PhoneVR/issues/26#issuecomment-683905486)
* Network Capture of Android/Mobile `.pcap` file. How-to-get-it-> [Here](https://github.com/PhoneVR-Developers/PhoneVR/issues/26#issuecomment-687640757)

<ins>If Unexpected android app crashes occur(unexpected means, you did **NOT** get</ins> [this](https://user-images.githubusercontent.com/4137788/102040730-9c168180-3df3-11eb-985a-5d8d6798ea5a.jpg) <ins>screen when PhoneVR crashed),</ins>

1. Open `PhoneVR` android app and let it crash.
2. Collect `bugreport-xxx.zip` from android device (How-to under *"Capture a bug report from a device"* paragraph [here](https://developer.android.com/studio/debug/bug-report#bugreportdevice))
3. Attach the `bugreport-xxx.zip` file in Discord or GitHub or mail to *"phonevr.crash@gmail.com"*

<ins>**Common files required for all kinds issues,**</ins>
- If using PhoneVR Legacy server
  * `pvrLog.txt` and/or `pvrDebugLog.txt` file(s) in default **windows** location `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\PVRServer\logs`
  * `pvrLog.txt` and/or `pvrDebugLog.txt` file(s) in default **android** location `.../Android/data/virtualis.phonevr/files/PVR/`. *Optionally* you can also attach `Log` from your `Settings page` on the app.
- If using ALVR
  * ALVR logs from its server, are available in the GUI (you may which to enable Setting -> Logging -> [Log to disk](https://github.com/alvr-org/ALVR/wiki/Settings-guide#log-to-disk) if you want to keep saving the logs to file, even after alvr is closed 
* Screenshots / Videos are much more helpful for developers to understand the issues faster.
* And ofcourse, how to reproduce the issue :)
