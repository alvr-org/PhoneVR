/* (C)2024 */
package viritualisres.phonevr;

import android.content.SharedPreferences;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.Log;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

public class Passthrough implements SensorEventListener {

    static {
        System.loadLibrary("native-lib-alvr");
    }

    private static final String TAG = Passthrough.class.getSimpleName() + "-Java";

    private Camera mCamera = null;

    private SurfaceTexture mTexture = null;
    private Camera.Size mPreviewSize = null;

    private final SharedPreferences mPref;

    private final SensorManager mSensorManager;

    private GraphAdapter graphAdapter = null;

    private final int displayWidth;

    private final int displayHeight;

    private Sensor mAccelerometer = null;

    private long timestamp = 0;

    private long timestampAfterCandidate = 0;

    private long passthroughDelayInMs = 600;

    private float detectionLowerBound = 0.8f;

    private float detectionUpperBound = 4f;

    private final List<Float>[] accelStore =
            new List[] {new ArrayList<>(), new ArrayList<>(), new ArrayList<>()};

    private int halfSize = 0;

    private boolean triggerIsSet = false;

    private final List<Float> cumSums = new ArrayList<>();

    public Passthrough(
            SharedPreferences pref,
            SensorManager sensorManager,
            int displayWidth,
            int displayHeight) {
        this.displayWidth = displayWidth;
        this.displayHeight = displayHeight;
        mPref = pref;
        mSensorManager = sensorManager;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mAccelerometer =
                    mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER_UNCALIBRATED);
        }
        if (mAccelerometer == null) {
            mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (timestamp == 0) {
            timestamp = event.timestamp;
        }

        if (timestampAfterCandidate == 0) {
            timestampAfterCandidate = event.timestamp - 1;
        }

        boolean initialFull = event.timestamp - timestamp > passthroughDelayInMs * 1000000L;
        boolean afterCandidateFull =
                event.timestamp - timestampAfterCandidate > passthroughDelayInMs * 1000000L;

        // gather data for passthrough_delay_ms, we get a data point approx. each 60ms
        String axesName[] = {"X", "Y", "Z"};
        for (int i = 0; i < accelStore.length; ++i) {
            // derivative, absolute
            accelStore[i].add(event.values[i]);

            if (initialFull) {
                // remove the first element to only store the last X elements
                accelStore[i].remove(0);
                if (halfSize == 0) {
                    halfSize = accelStore[0].size() / 2;
                }
                // check if half size has at least 3 data points
                if (halfSize < 4) {
                    accelStore[i].clear();
                    timestamp = 0;
                    halfSize = 0;
                }
            }
        }

        if (halfSize > 0) {
            // Calculate derivative and absolute values
            List<Float>[] accelAbs =
                    new List[] {new ArrayList<>(), new ArrayList<>(), new ArrayList<>()};
            for (int axis = 0; axis < accelStore.length; ++axis) {
                for (int i = 1; i < accelStore[axis].size(); ++i) {
                    accelAbs[axis].add(
                            Math.abs(accelStore[axis].get(i) - accelStore[axis].get(i - 1)));
                }
                if (graphAdapter != null) {
                    graphAdapter.addValue(
                            event.timestamp,
                            axesName[axis],
                            accelAbs[axis].get(accelAbs[axis].size() - 1));
                }
            }

            // get the mean for each axis and sum them up
            float[] axisMean = new float[] {0.0f, 0.0f, 0.0f};
            for (int i = 0; i < accelAbs.length; ++i) {
                for (int j = 0; j < accelAbs[i].size(); ++j) {
                    axisMean[i] += accelAbs[i].get(j);
                }
                axisMean[i] /= accelAbs[i].size();
            }
            float sumMean = axisMean[0] + axisMean[1] + axisMean[2];
            cumSums.add(sumMean);
            if (graphAdapter != null) {
                graphAdapter.addValue(event.timestamp, "sumMean", sumMean);
            }

            if (cumSums.size() < accelAbs[0].size()) {
                return;
            } else {
                // only store the last X elements
                cumSums.remove(0);
            }

            if (!afterCandidateFull) {
                // if a potential passthrough_change event is found,
                // check if the device does not move much anymore
                if (!triggerIsSet && sumMean < detectionLowerBound) {
                    triggerIsSet = true;
                    // if passthroughMode not already changed... change it
                    changePassthroughMode();
                }
            } else {
                triggerIsSet = false;
                // here we check for potential passthrough_change events
                // these may occur on any axis
                for (List<Float> elem : accelAbs) {
                    // we look at two halves of the passthrough_delay
                    List<Float> t1 = elem.subList(0, halfSize + 1);
                    List<Float> t2 = elem.subList(halfSize, elem.size());

                    // and get the maxima
                    float max1 = Collections.max(t1);
                    float max2 = Collections.max(t2);
                    // and check whether the 2nd half goes belowe lower bound
                    int end2 = t2.indexOf(max2);
                    for (int i = end2; i < t2.size(); ++i) {
                        if (t2.get(i) < detectionLowerBound) {
                            end2 = i;
                            break;
                        }
                    }

                    // We check
                    if (t1.get(halfSize - 1) > t2.get(0) // if halfSize is a local minimum
                            && t2.get(1) > t2.get(0)
                            && max1 - t1.get(0) >= detectionLowerBound // max - start > lower bound
                            && max1 - t1.get(t1.size() - 1)
                                    >= detectionLowerBound // max - end > lower bound
                            && max2 - t2.get(0) >= detectionLowerBound
                            && max2 - t2.get(t2.size() - 1) >= detectionLowerBound
                            && max1 >= detectionLowerBound // max are in bounds
                            && max1 <= detectionUpperBound
                            && max2 >= detectionLowerBound
                            && max2 <= detectionUpperBound
                            && sumMean >= detectionLowerBound // sumMean is in bounds
                            && sumMean <= detectionUpperBound
                            && t2.get(end2) < max2 // the 2nd half goes below lower bound
                            && cumSums.get(0)
                                    < Math.max(
                                            detectionLowerBound - 0.3f,
                                            0.5f) // the first sumMean (of window) is below lower
                    // bounds
                    ) {
                        // If all is fulfilled we consider this a potential passthrough_change
                        // event.
                        // The sum must then decend under lower bound until passthrough_delay
                        timestampAfterCandidate = event.timestamp;
                        break;
                    }
                }
            }
        }
        if (graphAdapter != null) {
            graphAdapter.addValue(
                    event.timestamp, "Candidate", timestampAfterCandidate == event.timestamp);
            graphAdapter.addValue(event.timestamp, "Trigger", triggerIsSet);
            graphAdapter.addValue(event.timestamp, "upperBound", detectionUpperBound);
            graphAdapter.addValue(event.timestamp, "lowerBound", detectionLowerBound);
            graphAdapter.refresh();
            this.updateSettings();
        }

        Log.v(
                TAG + "-AccSensor",
                String.format(
                        (Locale) null,
                        "%d: [%f, %f, %f]",
                        event.timestamp,
                        event.values[0],
                        event.values[1],
                        event.values[2]));
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    protected void onResume(GraphAdapter graphAdapter) {
        this.graphAdapter = graphAdapter;
        this.onResume();
    }

    protected void updateSettings() {
        try {
            passthroughDelayInMs = Long.parseLong(mPref.getString("passthrough_delay", "600"));
        } catch (Exception e) {
            passthroughDelayInMs = 600;
        }
        try {
            detectionLowerBound = Float.parseFloat(mPref.getString("passthrough_lower", "0.8"));
        } catch (Exception e) {
            detectionLowerBound = 0.8f;
        }
        try {
            detectionUpperBound = Float.parseFloat(mPref.getString("passthrough_upper", "4"));
        } catch (Exception e) {
            detectionUpperBound = 4.f;
        }
    }

    protected void onResume() {
        SharedPreferences.Editor edit = mPref.edit();
        edit.putBoolean("passthrough", false);
        edit.apply();

        timestamp = 0;
        halfSize = 0;
        if (mPref.getBoolean("passthrough_tap", true)) {
            updateSettings();
            if (mAccelerometer != null) {
                mSensorManager.registerListener(
                        this, mAccelerometer, SensorManager.SENSOR_DELAY_UI);
            }
        } else {
            mSensorManager.unregisterListener(this);
        }
    }

    protected void setPassthroughMode(boolean passthrough) {
        if (passthrough) {
            int w = displayWidth / 2;
            int h = displayHeight;
            if (displayWidth < displayHeight) {
                w = displayHeight / 2;
                h = displayWidth;
            }
            openCamera(-1, w, h, mPref.getBoolean("passthrough_recording", true));

            float size = 0.5f;
            try {
                size = Float.parseFloat(mPref.getString("passthrough_fraction", "0.5"));
            } catch (RuntimeException e) {
            }

            setPassthroughSizeNative(size);
        }
        setPassthroughActiveNative(passthrough);

        if (passthrough) {
            startPreview();
        } else {
            releaseCamera();
        }
    }

    protected void changePassthroughMode() {
        boolean pt = !mPref.getBoolean("passthrough", false);
        SharedPreferences.Editor edit = mPref.edit();
        edit.putBoolean("passthrough", pt);
        edit.apply();
        setPassthroughMode(pt);
    }

    public void createTexture(int textureID) {
        mTexture = new SurfaceTexture(textureID);
    }

    public void openCamera(int camNr, int desiredWidth, int desiredHeight, boolean recordingHint) {
        if (mCamera != null) {
            // TODO maybe print only a warning
            throw new RuntimeException("Camera already initialized");
        }
        Camera.CameraInfo info = new Camera.CameraInfo();
        // TODO add possibility to choose camera
        if (camNr < 0) {
            int numCameras = Camera.getNumberOfCameras();
            for (int i = 0; i < numCameras; ++i) {
                Camera.getCameraInfo(i, info);
                if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
                    mCamera = Camera.open(i);
                    break;
                }
            }
            if (mCamera == null) {
                // TODO log message "No back-facing camera found; opening default"
                mCamera = Camera.open(); // open default
            }
        } else {
            mCamera = Camera.open(camNr);
        }
        if (mCamera == null) {
            throw new RuntimeException("Unable to open camera");
        }
        // TODO the size stuff
        Camera.Parameters params = mCamera.getParameters();
        List<Camera.Size> choices = params.getSupportedPreviewSizes();
        mPreviewSize = chooseOptimalSize(choices, desiredWidth, desiredHeight);
        Log.d(TAG, "PreviewSize: " + mPreviewSize.width + "x" + mPreviewSize.height);
        params.setPreviewSize(mPreviewSize.width, mPreviewSize.height);
        params.setRecordingHint(recordingHint); // Can increase fps in passthrough mode
        mCamera.setParameters(params);
    }

    private Camera.Size chooseOptimalSize(List<Camera.Size> choices, int width, int height) {
        Integer minSize = Integer.max(Integer.min(width, height), 320);

        // Collect the supported resolutions that are at least as big as the preview Surface
        ArrayList<Camera.Size> bigEnough = new ArrayList<>();
        for (Camera.Size option : choices) {
            if (option.width == width && option.height == height) {
                return option;
            }
            if (option.height >= minSize && option.width >= minSize) {
                bigEnough.add(option);
            }
        }

        Comparator<Camera.Size> comp = (a, b) -> (a.width * a.height) - (b.width * b.width);
        // Pick the smallest of those, assuming we found any
        if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, comp);
        } else {
            return choices.get(0);
        }
    }

    public void releaseCamera() {
        if (mCamera != null) {
            mCamera.setPreviewCallback(null);
            mCamera.stopPreview();
            mCamera.release();
            mCamera = null;
        }
    }

    public void startPreview() {
        if (mCamera != null) {
            // Log.d(TAG, "starting camera preview")
            try {
                mCamera.setPreviewTexture(mTexture);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            mCamera.startPreview();
        }
    }

    public void onPause() {
        mSensorManager.unregisterListener(this);
        setPassthroughActiveNative(false);
        releaseCamera();
    }

    public void update() {
        if (mTexture != null) {
            mTexture.updateTexImage();
        }
    }

    private native void setPassthroughSizeNative(float size);

    private native void setPassthroughActiveNative(boolean activate);
}
