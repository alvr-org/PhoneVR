package viritualisres.phonevr


import android.content.Context
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_settings.*
import java.util.*

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        loadPrefs()
    }

    override fun onPause() {
        super.onPause()
        savePrefs()
    }

    private fun savePrefs() {
        val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)
        val edit = prefs.edit()
        with(edit) {
            putString(pcIpKey, pcIp.text.toString())
            putInt(connPortKey, connPort.text.toString().toInt())
            putInt(videoPortKey, videoPort.text.toString().toInt())
            putInt(posePortKey, posePort.text.toString().toInt())
            putInt(resMulKey, resMul.text.toString().toInt())
            putFloat(mt2phKey, mt2ph.text.toString().toFloat())
            putFloat(offFovKey, offFov.text.toString().toFloat())
            putBoolean(warpKey, warp.isChecked)
            putBoolean(debugKey, debug.isChecked)
            apply()
        }
    }

    private fun loadPrefs() {
        val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)

        val l = Locale.getDefault()
        val fmt = "%1\$d"
        val fmt2 = "%1$.1f"
        val fmt3 = "%1$.3f"
        pcIp.setText(prefs.getString(pcIpKey, pcIpDef))
        connPort.setText(String.format(l, fmt, prefs.getInt(connPortKey, connPortDef)))
        videoPort.setText(String.format(l, fmt, prefs.getInt(videoPortKey, videoPortDef)))
        posePort.setText(String.format(l, fmt, prefs.getInt(posePortKey, posePortDef)))
        resMul.setText(String.format(l, fmt, prefs.getInt(resMulKey, resMulDef)))
        mt2ph.setText(String.format(l, fmt3, prefs.getFloat(mt2phKey, mt2phDef)))
        offFov.setText(String.format(l, fmt2, prefs.getFloat(offFovKey, offFovDef)))
        warp.isChecked = prefs.getBoolean(warpKey, warpDef)
        debug.isChecked = prefs.getBoolean(debugKey, debugDef)
    }
}