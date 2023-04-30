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
@RunWith(AndroidJUnit4::class)
class ALVRActivityTest {

    private val TAG: String? = javaClass.simpleName

    @get:Rule
    var perms2:GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.WRITE_EXTERNAL_STORAGE);

    @get:Rule
    var perms8: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.RECORD_AUDIO);

    @get:Rule
    var perms7: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.CAMERA);
    /*@get:Rule
    var perms3: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.VIBRATE);

    @get:Rule
    var perms1:GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.READ_EXTERNAL_STORAGE);

    @get:Rule
    var perms4: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.NFC);

    @get:Rule
    var perms5: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.INTERNET);

    @get:Rule
    var perms6: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.READ_LOGS);

    @get:Rule
    var perms9: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.WAKE_LOCK);

    @get:Rule
    var perms10: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.ACCESS_WIFI_STATE);*/

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

    companion object {
        // This is a specific fix for CIs (github-actions, etc) to close ANR dialog
        @JvmStatic
        @BeforeClass
        fun dismissANRSystemDialog() {
            val device = UiDevice.getInstance(getInstrumentation())
            // If running the device in English Locale
            val waitButton = device.findObject(UiSelector().textContains("wait"))
            if (waitButton.exists()) {
                waitButton.click()
            }
        }
    }

    @SuppressLint("CheckResult")
    @Test
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