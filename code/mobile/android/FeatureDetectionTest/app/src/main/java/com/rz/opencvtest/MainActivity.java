package com.rz.opencvtest;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Size;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends AppCompatActivity implements SensorEventListener {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }


    int whichView = 0;


    Size imgSize = new Size(1280, 720);
    int outFmt = 0;

    Handler hdlr;
    HandlerThread hdlrThr;
    GLSurfaceView glsv;

    Surface surf;
    SensorManager sm;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        glsv = (GLSurfaceView) findViewById(R.id.textureView);
        glsv.setEGLContextClientVersion(3);
        //glsv.setEGLConfigChooser(8, 8, 8, 0, 0, 0);
        glsv.setPreserveEGLContextOnPause(true);
        glsv.setRenderer(new Renderer());


        findViewById(R.id.show).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                whichView = (whichView + 1) % 3;
            }
        });

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.CAMERA}, 1);
        }

        sm = (SensorManager) getSystemService(SENSOR_SERVICE);
    }

    private void setupCamera() {
        if(ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)
            return;
        try {
            CameraManager manager = (CameraManager)getSystemService(Context.CAMERA_SERVICE);
            String camID = "";
            for (String id : manager.getCameraIdList()){
                CameraCharacteristics chrs = manager.getCameraCharacteristics(id);
                int[] sad = chrs.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);

                //int fdsfsd = chrs.get(CameraCharacteristics.SENSOR_ORIENTATION);
                if(chrs.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK) { // cannot get rid of unboxing warning!
                    StreamConfigurationMap map = chrs.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                    //chrs.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
                    assert map != null;
//                    if (map.isOutputSupportedFor(ImageFormat.YUV_420_888))
//                        outFmt = ImageFormat.YUV_420_888;
//                    else

                    if (map.isOutputSupportedFor(ImageFormat.JPEG))
                        outFmt = ImageFormat.JPEG;

                    //Size[] dsdjkla = map.getOutputSizes()

                    imgSize = map.getOutputSizes(outFmt)[0]; // TODO: find closest size to screen size
                    //map.get
                    //map.getOutputSizes widths are always > than heights.

                    camID = id;
                    break;
                }
            }

            manager.openCamera(camID, new CameraDevice.StateCallback() {
                CameraDevice cam;
                CaptureRequest.Builder capReqBuilder;

                @Override
                public void onOpened(@NonNull CameraDevice camera) {
                    cam = camera;
                    try {
                        capReqBuilder = cam.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);//CameraDevice.TEMPLATE_RECORD TEMPLATE_PREVIEW
                        //capReqBuilder.get(CaptureRequest)
                        //capReqBuilder.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_OFF);
                        //capReqBuilder.set(CaptureRequest, CameraMetadata.CONTROL_MODE_OFF);
                        //capReqBuilder.set(CaptureRequest.);
                        //capReqBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_AUTO);
                        //capReqBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                        //capReqBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_OFF);
                        //capReqBuilder.set(CaptureRequest.SENSOR_FRAME_DURATION, 1000000000L);

                        //capReqBuilder.set(CaptureRequest.SENSOR_EXPOSURE_TIME, 1000L);
                        //capReqBuilder.set(CaptureRequest.JPEG_ORIENTATION, ORIENTATIONS.get(rotation));
                        capReqBuilder.addTarget(surf);
                        //CaptureRequest.Fps

                        cam.createCaptureSession(Arrays.asList(surf), new CameraCaptureSession.StateCallback(){
                            @Override
                            public void onConfigured(@NonNull CameraCaptureSession capSes) {
                                try {
                                    capSes.setRepeatingRequest(capReqBuilder.build(), null, hdlr);
                                } catch (CameraAccessException e) {
                                    e.printStackTrace();
                                }
                            }
                            @Override
                            public void onConfigureFailed(@NonNull CameraCaptureSession capSes) {
                                Toast.makeText(MainActivity.this, "Configuration change", Toast.LENGTH_SHORT).show();
                            }
                        }, hdlr);
                    } catch (CameraAccessException e) {
                        e.printStackTrace();
                    }
                }
                @Override
                public void onDisconnected(@NonNull CameraDevice camera) {
                    cam.close();
                    cam = null;
                }
                @Override
                public void onError(@NonNull CameraDevice camera, int error) {
                    cam.close();
                    cam = null;
                }
            }, hdlr);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

    }

    @Override
    protected void onResume() {
        super.onResume();
        Sensor orient = sm.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR); //Sensor.TYPE_ROTATION_VECTOR for orientation
        sm.registerListener(this, orient, SensorManager.SENSOR_DELAY_FASTEST, 5000); //max 5ms latency
        Sensor acc = sm.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION); //Sensor.TYPE_ROTATION_VECTOR for orientation
        sm.registerListener(this, acc, SensorManager.SENSOR_DELAY_FASTEST, 5000); //max 5ms latency

        glsv.onResume();

        if (hdlrThr == null) {
            hdlrThr = new HandlerThread("Camera Background");
            hdlrThr.start();
            hdlr = new Handler(hdlrThr.getLooper());
        }
    }

    @Override
    protected void onPause() {
        if (hdlrThr != null) {
            hdlrThr.quitSafely();
            try {
                hdlrThr.join();
                hdlrThr = null;
                hdlr = null;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        glsv.onPause();

        sm.unregisterListener(this);
        super.onPause();
    }

    public native void Draw(int w, int h, int which);
    public native int InitGL();

    @Override
    public void onSensorChanged(SensorEvent event) {
       // if (event.sensor.getType() == Sensor.TYPE_ROTATION_VECTOR)
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {

    }


    class Renderer implements GLSurfaceView.Renderer, SurfaceTexture.OnFrameAvailableListener{
        private SurfaceTexture surfTex;
        boolean frmAvail = false;
        int w, h;

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            int texID = InitGL();
            surfTex = new SurfaceTexture(texID, false); // todo: set to true
            surfTex.setDefaultBufferSize(1280, 720);
//            surfTex.setDefaultBufferSize(640, 360);
            surfTex.setOnFrameAvailableListener(this);
            surf = new Surface(surfTex);
            setupCamera();
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            if(frmAvail) {
                surfTex.updateTexImage();
                Draw(w, h, whichView);
            }
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            w = width;
            h = height;
        }

        @Override
        public void onFrameAvailable(SurfaceTexture st) {
            frmAvail = true;
        }
    }


    public static native void SetSensData(int which, float[] data, long ts);
}
