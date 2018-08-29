package com.rz.phonevr

import android.content.Context
import android.content.SharedPreferences
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.widget.CheckBox
import android.widget.EditText

import java.util.Locale

class SettingsActivity : AppCompatActivity() {

    internal var sp: SharedPreferences? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        sp = getSharedPreferences("pvr_globals", Context.MODE_PRIVATE)
        LoadPrefs()
    }

    override fun onPause() {
        super.onPause()
        SavePrefs()
    }

    internal fun SavePrefs() {
        val edit = sp!!.edit()
        edit.putString("pcIp", (findViewById(R.id.pcIp) as EditText).text.toString())
        edit.putInt("connPort", Integer.parseInt((findViewById(R.id.connPort) as EditText).text.toString()))
        edit.putInt("videoPort", Integer.parseInt((findViewById(R.id.videoPort) as EditText).text.toString()))
        edit.putInt("posePort", Integer.parseInt((findViewById(R.id.posePort) as EditText).text.toString()))
        edit.putInt("resMul", Integer.parseInt((findViewById(R.id.resMul) as EditText).text.toString()))
        edit.putFloat("mt2ph", java.lang.Float.parseFloat((findViewById(R.id.mt2ph) as EditText).text.toString()))
        edit.putFloat("offFov", java.lang.Float.parseFloat((findViewById(R.id.offFov) as EditText).text.toString()))
        edit.putBoolean("warp", (findViewById(R.id.warp) as CheckBox).isChecked)
        edit.putBoolean("debug", (findViewById(R.id.debug) as CheckBox).isChecked)
        edit.apply()
    }

    internal fun LoadPrefs() {
        val l = Locale.getDefault()
        val fmt = "%1\$d"
        val fmt2 = "%1$.1f"
        val fmt3 = "%1$.3f"
        (findViewById(R.id.pcIp) as EditText).setText(sp!!.getString("pcIp", JavaWrap.pcIp))
        (findViewById(R.id.connPort) as EditText).setText(String.format(l, fmt, sp!!.getInt("connPort", JavaWrap.connPort)))
        (findViewById(R.id.videoPort) as EditText).setText(String.format(l, fmt, sp!!.getInt("videoPort", JavaWrap.videoPort)))
        (findViewById(R.id.posePort) as EditText).setText(String.format(l, fmt, sp!!.getInt("posePort", JavaWrap.posePort)))
        (findViewById(R.id.resMul) as EditText).setText(String.format(l, fmt, sp!!.getInt("resMul", JavaWrap.resMul)))
        (findViewById(R.id.mt2ph) as EditText).setText(String.format(l, fmt3, sp!!.getFloat("mt2ph", JavaWrap.mt2ph)))
        (findViewById(R.id.offFov) as EditText).setText(String.format(l, fmt2, sp!!.getFloat("offFov", JavaWrap.offFov)))
        (findViewById(R.id.warp) as CheckBox).isChecked = sp!!.getBoolean("warp", JavaWrap.warp)
        (findViewById(R.id.debug) as CheckBox).isChecked = sp!!.getBoolean("debug", JavaWrap.debug)

    }

}
