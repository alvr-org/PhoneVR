package viritualisres.phonevr

import android.content.Intent
import android.util.Log
import android.view.Surface
import java.lang.ref.WeakReference

internal object Wrap {

    private var mainRef: WeakReference<MainActivity>? = null
    private var gameRef: WeakReference<GameActivity>? = null

    fun setMainView(view: MainActivity) {
        mainRef = WeakReference(view)
    }

    fun setGameView(view: GameActivity) {
        gameRef = WeakReference(view)
    }

    //callbacks
    @JvmStatic
    fun segueToGame() {
        val rotation = mainRef?.get()?.windowManager?.defaultDisplay?.rotation
        //Log.d("--PVR-Java--", "Wrapper Class Layout Orientation : " /*+ mainLayout.rotation.toString() + ", GVR: "*/ + rotation);
        val intent = Intent(mainRef?.get(), GameActivity::class.java)
        intent.putExtra("MAINLAYOUT_ROT", rotation)
        mainRef?.get()?.startActivity(intent)
    }

    @JvmStatic
    fun unwindToMain() {
        gameRef?.get()?.finish()
    }

    @JvmStatic
    fun updateTextViewFPS(fpsStreamRecv : Float, fpsDecoder : Float, fpsRenderer: Float,
                          cfpsSteamVRApp : Float, cfpsEncoder : Float, cfpsStreamer: Float) {
        //Log.d("--PVR-Java--", "updateTextViewFPS Wrapper : $fpsStreamRecv $fpsDecoder $fpsRenderer")
        gameRef?.get()?.updateFPS(fpsStreamRecv,fpsDecoder,fpsRenderer, cfpsSteamVRApp, cfpsEncoder, cfpsStreamer)
    }

    external fun createRenderer(gvrCtx: Long)

    external fun setVStreamPort(port: Int)

    external fun startStream()

    external fun initSystem(screenWidth: Int, screenHeight: Int, resMul: Int, offScreenFov: Float,
                            enableWarp: Boolean, enableDebug: Boolean): Int

    external fun drawFrame(pts: Long)

    external fun onTriggerEvent()

    external fun onPause()

    external fun onResume()

    external fun startSendSensorData(port: Int)

    external fun setAccData(data: FloatArray)

    external fun startAnnouncer(pcIP: String, port: Int)

    external fun stopAnnouncer()

    external fun startMediaCodec(s: Surface)

    external fun stopAll()

    external fun vFrameAvailable(): Long

    external fun setExtDirectory(dir: String, len: Int)
}