/* (C)2023 */
package viritualisres.phonevr

import android.annotation.SuppressLint
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.MotionEvent
import android.view.View.OnTouchListener
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import java.io.*
import java.util.*
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import viritualisres.phonevr.databinding.ActivitySettingsBinding

class SettingsActivity : AppCompatActivity() {
    private lateinit var binding: ActivitySettingsBinding

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)
        loadPrefs()

        val etLogView = findViewById<TextView>(R.id.etLogView)

        setTextToLogger(etLogView)
        etLogView.setHorizontallyScrolling(true)
        etLogView.setMovementMethod(ScrollingMovementMethod())
        etLogView.setFocusable(false)
        val touchListener = OnTouchListener { v, motionEvent ->
            v.parent.requestDisallowInterceptTouchEvent(true)
            when (motionEvent.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_UP -> v.parent.requestDisallowInterceptTouchEvent(false)
            }
            false
        }
        etLogView.setOnTouchListener(touchListener)

        val btShareLogs: Button = findViewById(R.id.btShareLogs)
        btShareLogs.setOnClickListener {
            val intentShareFile = Intent(Intent.ACTION_SEND)

            val pvrFilesFolder = getExternalFilesDir(null).toString() + "/PVR"
            val pvrZipFilesFolder = getExternalFilesDir(null).toString() + "/PVR.zip"

            val fileZip = File(pvrZipFilesFolder)
            val fileWithinMyDir = File(pvrFilesFolder)

            if (fileWithinMyDir.exists()) {
                if (fileWithinMyDir.listFiles()?.size!! > 1) {
                    if (fileZip.exists()) fileZip.delete()

                    zipFileAtPath(pvrFilesFolder, pvrZipFilesFolder)
                    intentShareFile.type = "application/zip"
                    intentShareFile.putExtra(
                        Intent.EXTRA_STREAM,
                        FileProvider.getUriForFile(
                            Objects.requireNonNull(applicationContext),
                            BuildConfig.APPLICATION_ID + ".provider",
                            fileZip))
                } else {
                    intentShareFile.type = "application/txt"
                    intentShareFile.putExtra(
                        Intent.EXTRA_STREAM,
                        FileProvider.getUriForFile(
                            Objects.requireNonNull(applicationContext),
                            BuildConfig.APPLICATION_ID + ".provider",
                            File(getExternalFilesDir(null).toString() + "/PVR/pvrlog.txt")))
                }
                intentShareFile.putExtra(Intent.EXTRA_SUBJECT, "Sharing pvrlog File(s)...")
                intentShareFile.putExtra(Intent.EXTRA_TEXT, "Sharing pvrlog File(s)...")
                startActivity(Intent.createChooser(intentShareFile, "Share File"))
            }
        }

        val btOpenLogs: Button = findViewById(R.id.btOpenLogs)
        btOpenLogs.setOnClickListener {
            val dir = File(getExternalFilesDir(null).toString() + "/PVR/")
            if (dir.listFiles()?.size!! > 1) {
                val items = arrayOfNulls<CharSequence>(2)
                items[0] = "pvrlog.txt"
                items[1] = "pvrDebuglog.txt"

                val builder = AlertDialog.Builder(this@SettingsActivity)
                builder
                    .setTitle(R.string.pick_logfile)
                    .setItems(
                        R.array.logsFiles,
                        DialogInterface.OnClickListener { _, which ->
                            Log.d("PVR_JAVA", "Which $which")
                            OpenFileWithIntent(
                                File(getExternalFilesDir(null).toString() + "/PVR/" + items[which]))
                        })
                builder.create()
                builder.show()
            } else {
                OpenFileWithIntent(File(getExternalFilesDir(null).toString() + "/PVR/pvrlog.txt"))
            }
        }

        val btSwitchServer: Button = findViewById(R.id.btSwitchServer)
        btSwitchServer.setOnClickListener {
            val prefs = getSharedPreferences("prefs", MODE_PRIVATE)
            prefs.edit().remove("alvr_server").apply()

            // Start initActivity and let user choose the server again. Clear back history
            val intent = Intent(this@SettingsActivity, InitActivity::class.java)
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
            startActivity(intent)
        }
    }
    /*
     * Zips a file at a location and places the resulting zip file at the toLocation
     * Example: zipFileAtPath("downloads/myfolder", "downloads/myFolder.zip");
     */
    private fun zipFileAtPath(sourcePath: String, toLocation: String?): Boolean {
        val BUFFER = 2048
        val sourceFile = File(sourcePath)
        try {
            var origin: BufferedInputStream? = null
            val dest = FileOutputStream(toLocation)
            val out = ZipOutputStream(BufferedOutputStream(dest))
            if (sourceFile.isDirectory) {
                zipSubFolder(out, sourceFile, sourceFile.parent.length)
            } else {
                val data = ByteArray(BUFFER)
                val fi = FileInputStream(sourcePath)
                origin = BufferedInputStream(fi, BUFFER)
                val entry = ZipEntry(getLastPathComponent(sourcePath))
                entry.setTime(
                    sourceFile.lastModified()) // to keep modification time after unzipping
                out.putNextEntry(entry)
                var count: Int = 0
                while (origin.read(data, 0, BUFFER).also({ count = it }) != -1) {
                    out.write(data, 0, count)
                }
            }
            out.close()
        } catch (e: java.lang.Exception) {
            e.printStackTrace()
            return false
        }
        return true
    }

    /*
     * Zips a subfolder
     */
    @Throws(IOException::class)
    private fun zipSubFolder(out: ZipOutputStream, folder: File, basePathLength: Int) {
        val BUFFER = 2048
        val fileList = folder.listFiles()
        var origin: BufferedInputStream? = null
        for (file in fileList) {
            if (file.isDirectory) {
                zipSubFolder(out, file, basePathLength)
            } else {
                val data = ByteArray(BUFFER)
                val unmodifiedFilePath = file.path
                val relativePath = unmodifiedFilePath.substring(basePathLength)
                val fi = FileInputStream(unmodifiedFilePath)
                origin = BufferedInputStream(fi, BUFFER)
                val entry = ZipEntry(relativePath)
                entry.setTime(file.lastModified()) // to keep modification time after unzipping
                out.putNextEntry(entry)
                var count: Int = 0
                while (origin.read(data, 0, BUFFER).also({ count = it }) != -1) {
                    out.write(data, 0, count)
                }
                origin.close()
            }
        }
    }

    /*
     * gets the last path component
     *
     * Example: getLastPathComponent("downloads/example/fileToZip");
     * Result: "fileToZip"
     */
    private fun getLastPathComponent(filePath: String): String? {
        val segments = filePath.split("/".toRegex()).toTypedArray()
        return if (segments.size == 0) "" else segments[segments.size - 1]
    }

    private fun OpenFileWithIntent(file: File) {
        // Get URI and MIME type of file
        val uri =
            FileProvider.getUriForFile(
                Objects.requireNonNull(applicationContext),
                BuildConfig.APPLICATION_ID + ".provider",
                file)
        val mime = contentResolver.getType(uri)

        // Open file with user selected app
        val intent = Intent()
        intent.action = Intent.ACTION_VIEW
        intent.setDataAndType(uri, mime)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        startActivity(intent)
    }

    private fun setTextToLogger(et: TextView) {
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
            if (fileHandler != null)
                try {
                    fileHandler.close()
                } catch (e: IOException) {}
        }
    }

    override fun onPause() {
        super.onPause()
        savePrefs()
    }

    private fun savePrefs() {
        try {
            val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)
            val edit = prefs.edit()
            with(edit) {
                putString(pcIpKey, binding.pcIp.text.toString())
                if (Util_IsVaildPort(binding.connPort.text.toString().toInt()))
                    putInt(connPortKey, binding.connPort.text.toString().toInt())
                if (Util_IsVaildPort(binding.videoPort.text.toString().toInt()))
                    putInt(videoPortKey, binding.videoPort.text.toString().toInt())
                if (Util_IsVaildPort(binding.posePort.text.toString().toInt()))
                    putInt(posePortKey, binding.posePort.text.toString().toInt())
                putInt(resMulKey, binding.resMul.text.toString().toInt())
                putFloat(mt2phKey, binding.mt2ph.text.toString().replace(',', '.').toFloat())
                putFloat(offFovKey, binding.offFov.text.toString().replace(',', '.').toFloat())
                putBoolean(warpKey, binding.warp.isChecked)
                putBoolean(debugKey, binding.debug.isChecked)
                apply()
            }
        } catch (e: Exception) {
            Log.d("PVR-Java", "Exception caught in savePrefs.SettingsActivity : " + e.message)
            e.printStackTrace()
        }
    }

    private fun loadPrefs() {
        val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)

        val l = Locale.getDefault()
        val fmt = "%1\$d"
        val fmt2 = "%1$.1f"
        val fmt3 = "%1$.3f"
        binding.pcIp.setText(prefs.getString(pcIpKey, pcIpDef))
        binding.connPort.setText(String.format(l, fmt, prefs.getInt(connPortKey, connPortDef)))
        binding.videoPort.setText(String.format(l, fmt, prefs.getInt(videoPortKey, videoPortDef)))
        binding.posePort.setText(String.format(l, fmt, prefs.getInt(posePortKey, posePortDef)))
        binding.resMul.setText(String.format(l, fmt, prefs.getInt(resMulKey, resMulDef)))
        binding.mt2ph.setText(String.format(l, fmt3, prefs.getFloat(mt2phKey, mt2phDef)))
        binding.offFov.setText(String.format(l, fmt2, prefs.getFloat(offFovKey, offFovDef)))
        binding.warp.isChecked = prefs.getBoolean(warpKey, warpDef)
        binding.debug.isChecked = prefs.getBoolean(debugKey, debugDef)
    }

    private fun Util_IsVaildPort(port: Int): Boolean {
        return ((port > 0) && (port <= 65536))
    }
}
