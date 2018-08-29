package com.rz.phonevr

import android.content.Intent
import android.support.annotation.Keep
import android.view.Surface

import java.lang.ref.WeakReference

internal object JavaWrap {

    //defaults
    var pcIp = ""
    var connPort = 32415
    var videoPort = 15243
    var posePort = 51423
    var resMul = 120
    var mt2ph = 0.020f
    var offFov = 10.0f
    var warp = true
    var debug = false

    private var mainRef: WeakReference<MainActivity2>? = null
    private var gameRef: WeakReference<GameActivity>? = null

    fun setMainView(view: MainActivity2) {
        mainRef = WeakReference(view)
    }

    fun setGameView(view: GameActivity) {
        gameRef = WeakReference(view)
    }

    //callbacks
    @Keep
    fun segueToGame() {
        val intent = Intent(mainRef!!.get(), GameActivity::class.java)
        mainRef!!.get()!!.startActivity(intent)
    }

    @Keep
    fun unwindToMain() {
        if (gameRef!!.get() != null)
            gameRef!!.get()!!.finish()
    }

    external fun CreateRenderer(gvrCtx: Long)

    external fun SetVStreamPort(port: Int)

    external fun StartStream()

    external fun InitSystem(screenWidth: Int, screenHeight: Int, resMul: Int, offScreenFov: Float,
                            isDaydream: Boolean, enableWarp: Boolean, enableDebug: Boolean): Int

    external fun DrawFrame(pts: Long)

    external fun OnTriggerEvent()

    external fun OnPause()

    external fun OnResume()

    external fun StartSendSensorData(port: Int)

    external fun SetAccData(data: FloatArray)

    external fun StartAnnouncer(pcIP: String, port: Int)

    external fun StopAnnouncer()

    external fun StartMediaCodec(s: Surface)

    external fun StopAll()

    external fun VFrameAvailable(): Long
}
