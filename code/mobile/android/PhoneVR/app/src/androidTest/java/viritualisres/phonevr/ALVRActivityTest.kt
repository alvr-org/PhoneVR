package viritualisres.phonevr

import android.annotation.SuppressLint
import android.content.pm.ActivityInfo
import androidx.test.core.app.takeScreenshot
import androidx.test.core.graphics.writeToTestStorage
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.isRoot
import androidx.test.ext.junit.rules.activityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import androidx.test.uiautomator.UiDevice
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TestName
import org.junit.runner.RunWith
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
    // a handy JUnit rule that stores the method name, so it can be used to generate unique
    // screenshot files per test method
    @get:Rule
    var nameRule = TestName()

    @get:Rule
    val activityScenarioRule = activityScenarioRule<ALVRActivity>()

    /**
     * Captures and saves an image of the entire device screen to storage.
     */
    @SuppressLint("CheckResult")
    @Test
    @Throws(IOException::class)
    fun saveDeviceScreenBitmap() {
        onView(isRoot())
        sleep(100)

        takeScreenshot()
            .writeToTestStorage("${javaClass.simpleName}_${nameRule.methodName}")
    }
}