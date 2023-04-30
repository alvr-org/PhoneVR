package viritualisres.phonevr

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*
import org.junit.BeforeClass

@RunWith(AndroidJUnit4::class)
class AppPackageTest {

    companion object {
        // This is a specific fix for CIs (github-actions, etc) to close ANR dialog
        @JvmStatic
        @BeforeClass
        fun dismissANRSystemDialog() {
            val device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            // If running the device in English Locale
            val waitButton = device.findObject(UiSelector().textContains("wait"))
            if (waitButton.exists()) {
                waitButton.click()
            }
        }
    }
    @Test
    fun useAppContext() {
        // Context of the app under test.
        // Basic Test used to ensure that app starts up properly
        val appContext = InstrumentationRegistry.getInstrumentation().context
        assertEquals("viritualisres.phonevr", appContext.packageName.subSequence(0, 21))
    }
}
