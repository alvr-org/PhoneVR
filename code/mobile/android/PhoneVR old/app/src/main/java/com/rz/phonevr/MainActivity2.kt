package com.rz.phonevr

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.support.v7.widget.Toolbar
import android.util.Range
import android.view.Menu
import android.view.MenuItem

class MainActivity2 : AppCompatActivity() {

    private var sp: SharedPreferences? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        sp = getSharedPreferences("pvr_globals", Context.MODE_PRIVATE)
        JavaWrap.setMainView(this)

        val toolbar = findViewById(R.id.toolbar) as Toolbar
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
        JavaWrap.StartAnnouncer(sp!!.getString("pcIp", JavaWrap.pcIp), sp!!.getInt("connPort", JavaWrap.connPort))
    }

    override fun onPause() {
        super.onPause()
        JavaWrap.StopAnnouncer()
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
            //System.loadLibrary("gvr_audio");
            System.loadLibrary("native-lib")
        }
    }
}
