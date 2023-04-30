package viritualisres.phonevr.utils

import android.util.Log
import androidx.test.espresso.Espresso
import androidx.test.espresso.base.DefaultFailureHandler
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Rule
import java.util.Locale

open class PVRInstrumentationBase {
    //Running count of the number of Android Not Responding dialogues to prevent endless dismissal.
    private var anrCount = 0
    //`RootViewWithoutFocusException` class is private, need to match the message (instead of using type matching).
    private val rootViewWithoutFocusExceptionMsg = java.lang.String.format(
        Locale.ROOT,
        "Waited for the root of the view hierarchy to have "
                + "window focus and not request layout for 10 seconds. If you specified a non "
                + "default root matcher, it may be picking a root that never takes focus. "
                + "Root:")

    /*@get:Rule
    var perms2: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.WRITE_EXTERNAL_STORAGE);

    @get:Rule
    var perms8: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.RECORD_AUDIO);

    @get:Rule
    var perms7: GrantPermissionRule = GrantPermissionRule.grant(android.Manifest.permission.CAMERA);
    @get:Rule
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

    @Before
    fun setUpHandler() {
        Espresso.setFailureHandler { error, viewMatcher ->

            if (error.message!!.contains(rootViewWithoutFocusExceptionMsg) && anrCount < 3) {
                anrCount++
                dismissANRSystemDialog()
            } else { // chain all failures down to the default espresso handler
                DefaultFailureHandler(InstrumentationRegistry.getInstrumentation().targetContext).handle(error, viewMatcher)
            }
        }
    }

    companion object {
        // This is a specific fix for CIs (github-actions, etc) to close ANR dialog
        @JvmStatic
        @BeforeClass
        fun dismissANRSystemDialog() {
            Log.d("PVR-Test-Debug", "dismissANRSystemDialog: Called")
            val device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            // If running the device in English Locale
            val waitButton = device.findObject(UiSelector().textContains("wait"))
            if (waitButton.exists()) {
                waitButton.click()
            }
        }
    }
}