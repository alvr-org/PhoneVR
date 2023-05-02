package viritualisres.phonevr

import android.annotation.SuppressLint
import android.util.Log
import androidx.test.core.app.takeScreenshot
import androidx.test.core.graphics.writeToTestStorage
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.matcher.ViewMatchers.isRoot
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.rules.activityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import androidx.test.rule.GrantPermissionRule
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TestName
import org.junit.runner.RunWith
import viritualisres.phonevr.utils.PVRInstrumentationBase
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.lang.Thread.sleep


/*
 * SS Test All Activities Startup
 * Majorly from: https://github.com/android/testing-samples/blob/main/ui/espresso/ScreenshotSample/app/src/androidTest/java/com/example/android/testing/espresso/screenshotsample
 *
 * When this test is executed via gradle managed devices, the saved image files will be stored at
 * build/outputs/managed_device_android_test_additional_output/debugAndroidTest/managedDevice/nexusOneApi30/
 * or
 * build/outputs/connected_android_test_additional_output/debugAndroidTest/connected/Pixel_2_API_30(AVD) - 11
 */
// @RunWith(AndroidJUnit4::class)
class ALVRActivityTest: PVRInstrumentationBase() {

    private val TAG: String? = javaClass.simpleName

    @get:Rule
    var perms2:GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.WRITE_EXTERNAL_STORAGE);

    @get:Rule
    var perms8: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.RECORD_AUDIO);

    @get:Rule
    var perms7: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.CAMERA);

    @get:Rule
    var nameRule = TestName()

    @get:Rule
    val activityScenarioRule = activityScenarioRule<ALVRActivity>()

    private fun waitForADBTelnetServer() {

        val filesDir = InstrumentationRegistry.getInstrumentation().targetContext.filesDir
        val lockFilesDir = File("/sdcard/")
        val fd = File(filesDir, "pvr-adb-telnet")
        val fsLock = File(lockFilesDir, "pvr-adb-telnet.lock")

        while (fsLock.exists() || fd.exists())
        {
            sleep(500)
            Log.d(TAG, "waitForADBTelnetServer: Waiting for ADB to get free")
        }
    }

    private fun shutdownADBTelnetServer() {
        val filesDir = InstrumentationRegistry.getInstrumentation().targetContext.filesDir
        val fd = File(filesDir, "pvr-adb-telnet.sd")

        waitForADBTelnetServer()
        fd.createNewFile()
        val stream: FileOutputStream = FileOutputStream(fd)
        try {
            stream.write("meow".toByteArray())
        } finally {
            stream.close()
        }
        while (fd.exists())
        {
            sleep(500)
            Log.d(TAG, "shutdownADBTelnetServer: Waiting for ADBTelnetServer to shutdown...")
        }
        Log.d(TAG, "shutdownADBTelnetServer: ADBTelnetServer is down.")
    }
    private fun sendADBCommand(cmd: String) {
        /*
        - Android Android 30 Emulator AVD Device - 28-Apr-2023 Tested
        | Directory                                                                         | Writable | Readable |
        | --------------------------------------------------------------------------------- | -------- | -------- |
        | getExternal..Dir() - /storage/emulated/0/Android/data/viritualisres.phonevr/files | true     | true     |
        | ✅ getfilesDir() is /data/data/user/0/viritualisres.phonevr/files                 | true     | true     |
        | /data/local/tmp                                                                   | false    | false    |
        | ✅ /sdcard                                                                        | false    | true     |

        - ADB - adb shell run-as ...
        | Directory                                                                         | Writable                                                                                                                                           | Readable                                                                                                                                                 |
        | --------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
        | getExternal..Dir() - /storage/emulated/0/Android/data/viritualisres.phonevr/files | false (adb shell "run-as viritualisres.phonevr echo meow >> /storage/emulated/0/Android/data/viritualisres.phonevr/files/test"); non-run-as: false | false (adb shell "run-as viritualisres.phonevr cat /storage/emulated/0/Android/data/viritualisres.phonevr/files/test/pvr-adb-telnet"); non-run-as: false |
        | ✅ getfilesDir() is /data/data/user/0/viritualisres.phonevr/files                 | false (adb shell "run-as viritualisres.phonevr echo meow >> /data/data/viritualisres.phonevr/files/pvr-adb-telnet.lock"); non-run-as: false        | true (adb shell "run-as viritualisres.phonevr cat /data/data/viritualisres.phonevr/files/pvr-adb-telnet"); non-run-as: false                             |
        | /data/local/tmp                                                                   | true (adb shell "echo meow >> /data/local/tmp/test"); run-as: false                                                                                | true (adb shell "run-as viritualisres.phonevr cat /data/local/tmp/test"); non-run-as: true                                                               |
        | ✅ /sdcard                                                                        | true (adb shell "echo meow >> /sdcard/test"); run-as: false                                                                                        | true (adb shell "cat /sdcard/test"); run-as: false                                                                                                       |

        - run-as meaning the cmd is run with specifying adb shell "run-as <package> ..."
        - non-run-as meaning the cmd is run with not specifying package
        */

        val filesDir = InstrumentationRegistry.getInstrumentation().targetContext.filesDir
        val lockFilesDir = File("/sdcard/")
        Log.d(TAG, "sendADBCommand: filesDir is $filesDir isW: ${filesDir!!.canWrite()} isR: ${filesDir.canRead()}")
        Log.d(TAG, "sendADBCommand: lockFilesDir is $lockFilesDir isW: ${lockFilesDir.canWrite()} isR: ${lockFilesDir.canRead()}")
        //filesDir.mkdirs()

        val fd = File(filesDir, "pvr-adb-telnet")
        val fsLock = File(lockFilesDir, "pvr-adb-telnet.lock")
        Log.d(TAG, "sendADBCommand: fd: ${fd.absolutePath} isW: ${fd.canWrite()} | exists: ${fd.exists()} | size: ${fd.length()}")
        Log.d(TAG, "sendADBCommand: lockFd: ${fsLock.absolutePath} isW: ${fsLock.canWrite()} | exists: ${fsLock.exists()} | size: ${fsLock.length()}")

        waitForADBTelnetServer()

        if( !fsLock.exists() && !fd.exists())
        {
            // ADB TelnetServer will createlockfile wait 500ms <-> read cmdfile -> execute cmdfile -> delete cmdfile -> delete lockfile
            // JavaTestApp, neither lockfile exists nor cmdfile exists -> create cmdfile -> write cmdfile
            // JavaTestApp, else wait 500ms - Some race exists, its okay - Only a test not for end-user consumption
            Log.d(TAG, "sendADBCommand: file is not locked and does not exist, ADB is free to execute telnet cmd")
            fd.createNewFile()
            val stream: FileOutputStream = FileOutputStream(fd)
            try {
                stream.write(cmd.toByteArray())
            } finally {
                stream.close()
            }
        }
    }

    private fun lookDown() {
        // set gravity acceleration g in Z Direction
        sendADBCommand("sensor set acceleration 0:0:9.77622");
    }

    private fun rotateAVDLandscape() {
        // set gravity acceleration g in Y Direction - real Mock
        sendADBCommand("sensor set acceleration 9.77622:0:0");
    }

    @SuppressLint("CheckResult")
    // @Test TODO: Fix Crash related to OpenGL on MacOS - after pulling upstream changes (for extra info, bug in alvr_client_core's frames)
    //    05-01 07:58:27.665  7493  7493 F DEBUG   : backtrace:
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #00 pc 00000b99  [vdso] (__kernel_vsyscall+9)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #01 pc 0005ad68  /apex/com.android.runtime/lib/bionic/libc.so (syscall+40) (BuildId: 6e3a0180fa6637b68c0d181c343e6806)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #02 pc 00076511  /apex/com.android.runtime/lib/bionic/libc.so (abort+209) (BuildId: 6e3a0180fa6637b68c0d181c343e6806)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #03 pc 002ff2de  /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk (offset 0x150c000)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #04 pc 002ffe01  /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk (offset 0x150c000)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #05 pc 00300d6e  /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk (offset 0x150c000)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #06 pc 002904cf  /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk (offset 0x150c000) (alvr_render_lobby_opengl+127)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #07 pc 00062920  /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk!libnative-lib.so (offset 0x1e4f000) (Java_viritualisres_phonevr_ALVRActivity_renderNative+3968) (BuildId: 6c0e91c7da5ce75187f221277be2b84bed4be231)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #08 pc 00142132  /apex/com.android.art/lib/libart.so (art_quick_generic_jni_trampoline+82) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.666  7493  7493 F DEBUG   :       #09 pc 0013b922  /apex/com.android.art/lib/libart.so (art_quick_invoke_stub+338) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #10 pc 001d0381  /apex/com.android.art/lib/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+241) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #11 pc 00386701  /apex/com.android.art/lib/libart.so (art::interpreter::ArtInterpreterToCompiledCodeBridge(art::Thread*, art::ArtMethod*, art::ShadowFrame*, unsigned short, art::JValue*)+385) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.662  7333  7440 I CardboardSDK: PosePrediction::GetRotationFromGyroscope: Velocity really small, returning identity rotation.
    //    05-01 07:58:27.667  5178  7519 I Fitness : OnPackageChangedOperation got intent: Intent { act=android.intent.action.PACKAGE_CHANGED dat=package:com.google.android.gms flg=0x45000010 cmp=com.google.android.gms/.chimera.PersistentIntentOperationService (has extras) } [CONTEXT service_id=17 ]
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #12 pc 0037aa3e  /apex/com.android.art/lib/libart.so (bool art::interpreter::DoCall<false, false>(art::ArtMethod*, art::Thread*, art::ShadowFrame&, art::Instruction const*, unsigned short, art::JValue*)+1070) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #13 pc 007a4179  /apex/com.android.art/lib/libart.so (MterpInvokeDirect+633) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #14 pc 001358a1  /apex/com.android.art/lib/libart.so (mterp_op_invoke_direct+33) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #15 pc 0000399c  [anon:dalvik-classes3.dex extracted in memory from /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk!classes3.dex] (viritualisres.phonevr.ALVRActivity.access$300)
    //    05-01 07:58:27.667  7493  7493 F DEBUG   :       #16 pc 007a505e  /apex/com.android.art/lib/libart.so (MterpInvokeStatic+1454) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #17 pc 00135921  /apex/com.android.art/lib/libart.so (mterp_op_invoke_static+33) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #18 pc 00003880  [anon:dalvik-classes3.dex extracted in memory from /data/app/~~6OpgdUaw8JphsNXgyGM1Sg==/viritualisres.phonevr-NkgO07jPDvzXvA_dHjnXNA==/base.apk!classes3.dex] (viritualisres.phonevr.ALVRActivity$Renderer.onDrawFrame+4)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #19 pc 007a355e  /apex/com.android.art/lib/libart.so (MterpInvokeInterface+2126) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #20 pc 001359a1  /apex/com.android.art/lib/libart.so (mterp_op_invoke_interface+33) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #21 pc 0034a3b4  /system/framework/framework.jar (offset 0x92b000) (android.opengl.GLSurfaceView$GLThread.guardedRun+1092)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #22 pc 007a44ae  /apex/com.android.art/lib/libart.so (MterpInvokeDirect+1454) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #23 pc 001358a1  /apex/com.android.art/lib/libart.so (mterp_op_invoke_direct+33) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #24 pc 0034a9ac  /system/framework/framework.jar (offset 0x92b000) (android.opengl.GLSurfaceView$GLThread.run+48)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #25 pc 0036fb02  /apex/com.android.art/lib/libart.so (art::interpreter::Execute(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame&, art::JValue, bool, bool) (.llvm.16375758241455872412)+370) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #26 pc 00379b00  /apex/com.android.art/lib/libart.so (art::interpreter::EnterInterpreterFromEntryPoint(art::Thread*, art::CodeItemDataAccessor const&, art::ShadowFrame*)+176) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.668  7493  7493 F DEBUG   :       #27 pc 0078b325  /apex/com.android.art/lib/libart.so (artQuickToInterpreterBridge+1061) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #28 pc 0014220d  /apex/com.android.art/lib/libart.so (art_quick_to_interpreter_bridge+77) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #29 pc 0013b922  /apex/com.android.art/lib/libart.so (art_quick_invoke_stub+338) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #30 pc 001d0381  /apex/com.android.art/lib/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+241) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #31 pc 0062f37c  /apex/com.android.art/lib/libart.so (art::JValue art::InvokeVirtualOrInterfaceWithJValues<art::ArtMethod*>(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, art::ArtMethod*, jvalue const*)+620) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #32 pc 0062f595  /apex/com.android.art/lib/libart.so (art::JValue art::InvokeVirtualOrInterfaceWithJValues<_jmethodID*>(art::ScopedObjectAccessAlreadyRunnable const&, _jobject*, _jmethodID*, jvalue const*)+85) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #33 pc 00697701  /apex/com.android.art/lib/libart.so (art::Thread::CreateCallback(void*)+1537) (BuildId: 8191579dfafff37a5cbca70f9a73020f)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #34 pc 000e6974  /apex/com.android.runtime/lib/bionic/libc.so (__pthread_start(void*)+100) (BuildId: 6e3a0180fa6637b68c0d181c343e6806)
    //    05-01 07:58:27.669  7493  7493 F DEBUG   :       #35 pc 00078567  /apex/com.android.runtime/lib/bionic/libc.so (__start_thread+71) (BuildId: 6e3a0180fa6637b68c0d181c343e6806)
    @Throws(IOException::class)
    fun saveDeviceScreenBitmap() {
        // SKIP Cardboard API Asking to Scan QR
        try {
            onView(withText("SKIP")).perform(click())
        } catch (e: NoMatchingViewException) {
            // View is not in hierarchy - which is okay
        }

        rotateAVDLandscape()
        waitForADBTelnetServer() // Wait for ADBTelnetServer to execute rotation

        onView(isRoot())

        // Wait for Rendering to settle
        sleep(1000)

        Log.d(TAG, "saveDeviceScreenBitmap: Executed AVD Commands, Taking screenshot...")
        takeScreenshot()
            .writeToTestStorage("${javaClass.simpleName}_${nameRule.methodName}")

        lookDown()
        waitForADBTelnetServer() // Wait for ADBTelnetServer to execute rotation
        sleep(1000)

        takeScreenshot()
            .writeToTestStorage("${javaClass.simpleName}_${nameRule.methodName}_LookingDown")
        //shutdownADBTelnetServer()
    }
    /*
    Uncomment this as a separate Test: Issue, Reopening (warm start) crashes ALVRActivity
    with panicked at 'assertion failed: previous.is_none()', C:\Users\user\.cargo\registry\src\index.crates.io-6f17d22bba15001f\ndk-context-0.1.1\src\lib.rs:87:5
    */

    /*
    @SuppressLint("CheckResult")
    @Test
    @Throws(IOException::class)
    fun saveDeviceScreenBitmapLookingDown() {
        // SKIP Cardboard API Asking to Scan QR
        try {
            onView(withText("SKIP")).perform(click())
        } catch (e: NoMatchingViewException) {
            // View is not in hierarchy - which is okay
        }

        // Wait for Rendering to settle
        onView(isRoot())

        rotateAVDLandscape()
        waitForADBTelnetServer() // Wait for ADBTelnetServer to execute rotation
        sleep(1000) // The the Render settledown

        lookDown()
        waitForADBTelnetServer() // Wait for ADBTelnetServer to execute rotation
        sleep(1000)

        takeScreenshot()
            .writeToTestStorage("${javaClass.simpleName}_${nameRule.methodName}")
    }*/
}