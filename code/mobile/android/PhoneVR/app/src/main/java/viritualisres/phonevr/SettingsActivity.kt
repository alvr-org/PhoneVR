package viritualisres.phonevr


import android.content.Context
import android.os.Bundle
import android.os.Environment.getExternalStorageDirectory
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_settings.*
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.io.RandomAccessFile
import java.util.*

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        loadPrefs()

        setTextToLogger(findViewById<EditText>(R.id.etLogView))
    }

    private fun setTextToLogger(et: EditText){
        val file: File = File(getExternalFilesDir(null).toString() + "/PVR/pvrlog.txt")
        et.setText(tail2(file, 100))
    }

    fun tail2(file: File, lines: Int): String {
        var fileHandler: RandomAccessFile? = null
        return try {
            fileHandler = RandomAccessFile(file, "r")
            val fileLength = fileHandler.length() - 1
            val sb = java.lang.StringBuilder()
            var line = 0
            for (filePointer in fileLength downTo -1 + 1) {
                fileHandler.seek(filePointer)
                val readByte = fileHandler.readByte().toInt()
                if (readByte == 0xA) {
                    if (filePointer < fileLength) {
                        line += 1
                    }
                } else if (readByte == 0xD) {
                    if (filePointer < fileLength - 1) {
                        line += 1
                    }
                }
                if (line >= lines) {
                    break
                }
                sb.append(readByte.toChar())
            }
            val toString = sb.reverse().toString()
            toString
        } catch (e: FileNotFoundException) {
            e.printStackTrace()
            ""
        } catch (e: IOException) {
            e.printStackTrace()
            ""
        } finally {
            if (fileHandler != null) try {
                fileHandler.close()
            } catch (e: IOException) {
            }
        }
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