package viritualisres.phonevr

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.view.Menu
import android.view.MenuItem

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        Wrap.setMainView(this)

        val toolbar: Toolbar = findViewById(R.id.toolbar)
        setSupportActionBar(toolbar)


        //        MediaCodecInfo[] dmsadas = (new MediaCodecList(MediaCodecList.REGULAR_CODECS)).getCodecInfos();
        //        for (MediaCodecInfo mci : dmsadas){
        //            if (mci.toString() == "OMX.qcom.video.encoder.avc"){
        //                mci.getSupportedTypes();
        //            }
        //        }

        // MediaCodecList.getCodecInfos();
        //Range<Integer> dsfs =  MediaCodecInfo;

    }

    override fun onResume() {
        super.onResume()
        val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)
        //Wrap.startAnnouncer(prefs.getString(pcIpKey, pcIpDef), prefs.getInt(connPortKey, connPortDef))
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
