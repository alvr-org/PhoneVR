package com.example.ricca.spacewarptest;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Size;
import android.view.Surface;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Arrays;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    float flowMul = 1.f;
    int whichView = 0, whichShot = 1;
    boolean shot = false;

    Handler hdlr;
    HandlerThread hdlrThr;
    GLSurfaceView glsv;
    Surface surf;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViewById(R.id.shot).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (whichView == 0) {
                    whichShot = (whichShot + 1) % 2;
                    shot = true;
                    if (whichShot == 1) {
                        whichView = 1;
                    }
                }
                else {
                    whichView = 0;
                    whichShot = 1;
                }
            }
        });

        findViewById(R.id.show).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                whichView = whichView != 4 ? (whichView + 1) % 5 : 1; // 1-2-3-4 round robin
            }
        });
        ((SeekBar)findViewById(R.id.ratioBar)).setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                flowMul = (float)progress / 50.f;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });



        glsv = (GLSurfaceView) findViewById(R.id.textureView);
        glsv.setEGLContextClientVersion(3);
        glsv.setPreserveEGLContextOnPause(true);
        glsv.setRenderer(new Renderer());

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.CAMERA}, 1);
        }
    }


    private void setupCamera() {
        if(ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)
            return;
        try {
            CameraManager manager = (CameraManager)getSystemService(Context.CAMERA_SERVICE);
            String camID = "";
            for (String id : manager.getCameraIdList()){
                CameraCharacteristics chrs = manager.getCameraCharacteristics(id);
                //int fdsfsd = chrs.get(CameraCharacteristics.SENSOR_ORIENTATION);
                if(chrs.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK) {
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
                        capReqBuilder = cam.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
                        capReqBuilder.addTarget(surf);

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

        super.onPause();
    }

    public native int InitGL();
    public native void Draw(int w, int h, int which, float flowMul);
    public native void Shot(int which);

    class Renderer implements GLSurfaceView.Renderer, SurfaceTexture.OnFrameAvailableListener{
        private SurfaceTexture surfTex;
        boolean frmAvail = false;
        int w, h;

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            int texID = InitGL();
            surfTex = new SurfaceTexture(texID, false); // todo: set to true
            surfTex.setDefaultBufferSize(1920, 1080);
            surfTex.setOnFrameAvailableListener(this);
            surf = new Surface(surfTex);
            setupCamera();
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            if(frmAvail) {
                surfTex.updateTexImage();
                Draw(w, h, whichView, flowMul);
            }
            if (shot) {
                Shot(whichShot);
                shot = false;
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
}
