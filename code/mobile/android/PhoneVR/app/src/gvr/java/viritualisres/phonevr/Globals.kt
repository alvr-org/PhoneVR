/* (C)2023 */
package viritualisres.phonevr

import android.os.Build

val pvrPrefsKey = "pvr_globals"

// https://stackoverflow.com/questions/2799097/how-can-i-detect-when-an-android-application-is-running-in-the-emulator
val isProbablyRunningOnEmulator: Boolean by lazy {
    // Android SDK emulator
    return@lazy ((Build.MANUFACTURER == "Google" &&
        Build.BRAND == "google" &&
        ((Build.FINGERPRINT.startsWith("google/sdk_gphone_") &&
            Build.FINGERPRINT.endsWith(":user/release-keys") &&
            Build.PRODUCT.startsWith("sdk_gphone_") &&
            Build.MODEL.startsWith("sdk_gphone_"))
        // alternative
        ||
            (Build.FINGERPRINT.startsWith("google/sdk_gphone64_") &&
                (Build.FINGERPRINT.endsWith(":userdebug/dev-keys") ||
                    Build.FINGERPRINT.endsWith(":user/release-keys")) &&
                Build.PRODUCT.startsWith("sdk_gphone64_") &&
                Build.MODEL.startsWith("sdk_gphone64_"))))
    //
    ||
        Build.FINGERPRINT.startsWith("generic") ||
        Build.FINGERPRINT.startsWith("unknown") ||
        Build.MODEL.contains("google_sdk") ||
        Build.MODEL.contains("Emulator") ||
        Build.MODEL.contains("Android SDK built for x86"))
    //            //bluestacks
    //            || "QC_Reference_Phone" == Build.BOARD && !"Xiaomi".equals(Build.MANUFACTURER,
    // ignoreCase = true)
    //            //bluestacks
    //            || Build.MANUFACTURER.contains("Genymotion")
    //            || Build.HOST.startsWith("Build")
    //            //MSI App Player
    //            || Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic")
    //            || Build.PRODUCT == "google_sdk"
    //            // another Android SDK emulator check
    //            || SystemProperties.getProp("ro.kernel.qemu") == "1")
}

// keys
val pcIpKey = "pcIp"
// If running on Android Emulator auto set host ip
val pcIpDef = if (isProbablyRunningOnEmulator) "10.0.2.2" else ""

val connPortKey = "connPort"
val connPortDef = 33333

val videoPortKey = "videoPort"
val videoPortDef = 15243

val posePortKey = "posePort"
val posePortDef = 51423

val resMulKey = "resMul"
val resMulDef = 120

val mt2phKey = "mt2ph"
val mt2phDef = 0.020f

val offFovKey = "offFov"
val offFovDef = 10.0f

val warpKey = "warp"
val warpDef = false // ReProj

val debugKey = "debug"
val debugDef = false
