package viritualisres.phonevr

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

@RunWith(AndroidJUnit4::class)
class AppPackageTest {
    @Test
    fun useAppContext() {
        // Context of the app under test.
        // Basic Test used to ensure that app starts up properly
        val appContext = InstrumentationRegistry.getInstrumentation().context
        assertEquals("viritualisres.phonevr", appContext.packageName.subSequence(0, 21))
    }
}
