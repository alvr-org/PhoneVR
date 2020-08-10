package viritualisres.phonevr

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment.*
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar


class MainActivity : AppCompatActivity() {

    private val REQUEST_CODE: Int = 52142;

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        setExtDirinJNI();

        setContentView(R.layout.activity_main)
        Wrap.setMainView(this)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val toolbar: Toolbar = findViewById(R.id.toolbar)
        setSupportActionBar(toolbar)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            when (PackageManager.PERMISSION_DENIED) {
                checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE) -> {
                    requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE), REQUEST_CODE)
                }
            }
        }

        //        MediaCodecInfo[] dmsadas = (new MediaCodecList(MediaCodecList.REGULAR_CODECS)).getCodecInfos();
        //        for (MediaCodecInfo mci : dmsadas){
        //            if (mci.toString() == "OMX.qcom.video.encoder.avc"){
        //                mci.getSupportedTypes();
        //            }
        //        }

        // MediaCodecList.getCodecInfos();
        //Range<Integer> dsfs =  MediaCodecInfo;
        //Log.d("PhoneVR", "OnCreate called");
    }

    private fun setExtDirinJNI() {
        val dir = getExternalFilesDir(null).toString()
        Log.d("PhoneVR", "ExtDir: " + dir + ", ExtRead Only? :"+ isExternalStorageReadOnly() + ", ExtAvalb ?:"+ isExternalStorageAvailable())
        Wrap.setExtDirectory(dir, dir.length)
    }

    private fun isExternalStorageReadOnly(): Boolean {
        val extStorageState: String = getExternalStorageState()
        return MEDIA_MOUNTED_READ_ONLY == extStorageState
    }

    private fun isExternalStorageAvailable(): Boolean {
        val extStorageState: String = getExternalStorageState()
        return MEDIA_MOUNTED == extStorageState
    }

    override fun onRequestPermissionsResult(requestCode: Int,
                                            permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CODE -> {
                if ((grantResults.isEmpty() ||
                        grantResults[0] != PackageManager.PERMISSION_GRANTED) ||
                        grantResults[1] != PackageManager.PERMISSION_GRANTED) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE), REQUEST_CODE)
                    }
                }
                else
                    setExtDirinJNI()
                return
            }
            else -> {
                // Ignore all other requests.
            }
        }
    }

    override fun onResume() {
        super.onResume()
        setExtDirinJNI()
        val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)
        //Wrap.startAnnouncer(prefs.getString(pcIpKey, pcIpDef), prefs.getInt(connPortKey, connPortDef))

        //Log.d("PhoneVR", "OnResume called... Starting Announcer @ port :" + prefs.getInt(connPortKey, connPortDef).toString());
        prefs.getString(pcIpKey, pcIpDef)?.let { Wrap.startAnnouncer(it, prefs.getInt(connPortKey, connPortDef)) }
    }

    override fun onPause() {
        super.onPause()
        Wrap.stopAnnouncer()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_settings) {
            val intent = Intent(this, SettingsActivity::class.java)
            startActivity(intent)
            return true
        }

        return super.onOptionsItemSelected(item)
    }

    companion object {
        init {
            System.loadLibrary("gvr")
            System.loadLibrary("native-lib")
        }
    }
}
