package viritualisres.phonevr

import android.app.Activity
import android.content.Context
import android.graphics.Point
import android.graphics.SurfaceTexture
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.Surface
import android.view.View

//import com.google.vr.cardboard.SurfaceTextureManager;
import com.google.vr.ndk.base.AndroidCompat
import com.google.vr.ndk.base.GvrLayout

import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10


class GameActivity : Activity(), SensorEventListener {

    private val sensMgr = getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val prefs = getSharedPreferences(pvrPrefsKey, Context.MODE_PRIVATE)
    private val surf = GLSurfaceView(this)
    private val gvrLayout = GvrLayout(this)

    private var isDaydream = false

    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        Wrap.setGameView(this)

        setImmersiveSticky()
        window.decorView.setOnSystemUiVisibilityChangeListener { visibility ->
            if (visibility and View.SYSTEM_UI_FLAG_FULLSCREEN == 0)
                setImmersiveSticky()
        }

        Wrap.createRenderer(gvrLayout.gvrApi.nativeGvrContext)

        with(surf) {
            setEGLContextClientVersion(3)
            setEGLConfigChooser(8, 8, 8, 0, 0, 0)
            preserveEGLContextOnPause = true
            setRenderer(Renderer())
            setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    //((Vibrator) getSystemService(Context.VIBRATOR_SERVICE)).vibrate(50);
                    Wrap.onTriggerEvent()
                    return@setOnTouchListener true
                }
                false
            }
        }
        gvrLayout.setPresentationView(surf)
        gvrLayout.uiLayout.setCloseButtonListener { onBackPressed() }
        setContentView(gvrLayout)
        if (gvrLayout.setAsyncReprojectionEnabled(true))
            AndroidCompat.setSustainedPerformanceMode(this, true)


        isDaydream = AndroidCompat.setVrModeEnabled(this, true)

        //getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        Wrap.startSendSensorData(prefs.getInt(posePortKey, posePortDef))

    }

    override fun onPause() {
        Wrap.onPause()
        surf.onPause()
        gvrLayout.onPause()
        sensMgr.unregisterListener(this)
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        val acc = sensMgr.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION) //Sensor.TYPE_ROTATION_VECTOR for orientation
        sensMgr.registerListener(this, acc, SensorManager.SENSOR_DELAY_FASTEST, 1000) //max 5ms latency
        gvrLayout.onResume()
        surf.onResume()
        Wrap.onResume()
    }

    override fun onDestroy() {
        Wrap.stopAll()
        gvrLayout.shutdown()
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus)
            setImmersiveSticky()
    }

    private fun setImmersiveSticky() {
        window.decorView.systemUiVisibility =
                (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }

    //SensorEventListener methods
    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    override fun onSensorChanged(event: SensorEvent) {
        Wrap.setAccData(event.values)
    }


    private inner class Renderer : GLSurfaceView.Renderer/*, SurfaceTexture.OnFrameAvailableListener*/ {
        private var surfTex: SurfaceTexture? = null

        override fun onSurfaceCreated(gl: GL10, c: EGLConfig) {
            val sz = Point()
            windowManager.defaultDisplay.getRealSize(sz)
            val texID = Wrap.initSystem(sz.x, sz.y,
                    prefs.getInt(resMulKey, resMulDef),
                    prefs.getFloat(offFovKey, offFovDef),
                    prefs.getBoolean(warpKey, warpDef),
                    prefs.getBoolean(debugKey, debugDef))

            surfTex = SurfaceTexture(texID, false) // true <- single buffer mode

            val s = Surface(surfTex)
            Wrap.startMediaCodec(s)
            s.release()

            Thread {
                Wrap.setVStreamPort(prefs!!.getInt(videoPortKey, videoPortDef))
                Wrap.startStream()
            }.start()
        }

        override fun onSurfaceChanged(gl: GL10, width: Int, height: Int) {}

        override fun onDrawFrame(gl: GL10) {
            val pts = Wrap.vFrameAvailable()
            if (pts != -1L)
                surfTex!!.updateTexImage()

            Wrap.drawFrame(pts)
        }
    }
}
