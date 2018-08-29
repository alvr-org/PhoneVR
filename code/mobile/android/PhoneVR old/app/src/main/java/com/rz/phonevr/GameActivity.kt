package com.rz.phonevr

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
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

    private var gvrLayout: GvrLayout? = null
    internal var surf: GLSurfaceView? = null
    internal var sm: SensorManager? = null

    internal var sp: SharedPreferences? = null

    internal var isDaydream = false

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        sp = getSharedPreferences("pvr_globals", Context.MODE_PRIVATE)

        JavaWrap.setGameView(this)

        setImmersiveSticky()
        window.decorView.setOnSystemUiVisibilityChangeListener { visibility ->
            if (visibility and View.SYSTEM_UI_FLAG_FULLSCREEN == 0) {
                setImmersiveSticky()
            }
        }

        gvrLayout = GvrLayout(this)
        JavaWrap.CreateRenderer(gvrLayout!!.gvrApi.nativeGvrContext)

        surf = GLSurfaceView(this)
        surf!!.setEGLContextClientVersion(3)
        surf!!.setEGLConfigChooser(8, 8, 8, 0, 0, 0)
        surf!!.preserveEGLContextOnPause = true
        surf!!.setRenderer(Renderer())
        surf!!.setOnTouchListener(View.OnTouchListener { v, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                //((Vibrator) getSystemService(Context.VIBRATOR_SERVICE)).vibrate(50);
                JavaWrap.OnTriggerEvent()
                return@OnTouchListener true
            }
            false
        })
        gvrLayout!!.setPresentationView(surf)
        gvrLayout!!.uiLayout.setCloseButtonListener { onBackPressed() }
        setContentView(gvrLayout)

        if (gvrLayout!!.setAsyncReprojectionEnabled(true))
            AndroidCompat.setSustainedPerformanceMode(this, true)


        isDaydream = AndroidCompat.setVrModeEnabled(this, true)

        //getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        sm = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        JavaWrap.StartSendSensorData(sp!!.getInt("posePort", JavaWrap.posePort))

    }

    override fun onPause() {
        JavaWrap.OnPause()
        surf!!.onPause()
        gvrLayout!!.onPause()
        sm!!.unregisterListener(this)
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        val acc = sm!!.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION) //Sensor.TYPE_ROTATION_VECTOR for orientation
        sm!!.registerListener(this, acc, SensorManager.SENSOR_DELAY_FASTEST, 1000) //max 5ms latency
        gvrLayout!!.onResume()
        surf!!.onResume()
        JavaWrap.OnResume()
    }

    override fun onDestroy() {
        JavaWrap.StopAll()
        gvrLayout!!.shutdown()
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus)
            setImmersiveSticky()
    }

    private fun setImmersiveSticky() {
        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }

    //SensorEventListener methods
    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    override fun onSensorChanged(event: SensorEvent) {
        JavaWrap.SetAccData(event.values)
    }


    private inner class Renderer : GLSurfaceView.Renderer/*, SurfaceTexture.OnFrameAvailableListener*/ {
        private var surfTex: SurfaceTexture? = null

        override fun onSurfaceCreated(gl: GL10, c: EGLConfig) {
            val sz = Point()
            windowManager.defaultDisplay.getRealSize(sz)
            val texID = JavaWrap.InitSystem(sz.x, sz.y,
                    sp!!.getInt("resMul", JavaWrap.resMul),
                    sp!!.getFloat("offFov", JavaWrap.offFov),
                    isDaydream,
                    sp!!.getBoolean("warp", JavaWrap.warp),
                    sp!!.getBoolean("debug", JavaWrap.debug))

            surfTex = SurfaceTexture(texID, false) // true <- single buffer mode

            val s = Surface(surfTex)
            JavaWrap.StartMediaCodec(s)
            s.release()
            object : Thread() {
                override fun run() {
                    JavaWrap.SetVStreamPort(sp!!.getInt("videoPort", JavaWrap.videoPort))
                    JavaWrap.StartStream()
                }
            }.start()
        }

        override fun onSurfaceChanged(gl: GL10, width: Int, height: Int) {}

        override fun onDrawFrame(gl: GL10) {
            val pts = JavaWrap.VFrameAvailable()
            if (pts != -1L)
                surfTex!!.updateTexImage()

            JavaWrap.DrawFrame(pts)
        }
    }
}
