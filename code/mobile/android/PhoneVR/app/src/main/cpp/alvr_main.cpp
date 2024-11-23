#include "alvr_client_core.h"
#include "arcore_c_api.h"
#include "cardboard.h"
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <algorithm>
#include <android/log.h>
#include <deque>
#include <jni.h>
#include <map>
#include <thread>
#include <unistd.h>
#include <vector>

#include "nlohmann/json.hpp"
#include "utils.h"

using namespace nlohmann;

uint64_t HEAD_ID = alvr_path_string_to_id("/user/head");

// TODO: Make this configurable.
// Using ARCore orientation is more accurate, but causes a ~0.5 second delay,
// which is probably nauseating for most folks. TODO.
bool useARCoreOrientation = false;

// Note: the Cardboard SDK cannot estimate display time and an heuristic is used instead.
const uint64_t VSYNC_QUEUE_INTERVAL_NS = 50e6;
const float FLOOR_HEIGHT = 1.5;
const int MAXIMUM_TRACKING_FRAMES = 360;

struct NativeContext {
    JavaVM *javaVm = nullptr;
    jobject javaContext = nullptr;

    CardboardHeadTracker *headTracker = nullptr;
    CardboardLensDistortion *lensDistortion = nullptr;
    CardboardDistortionRenderer *distortionRenderer = nullptr;

    bool arcoreEnabled = false;
    ArSession *arSession = nullptr;
    ArFrame *arFrame = nullptr;
    ArAnchor *arFloorAnchor = nullptr;
    GLuint arTexture = 0;

    AlvrQuat lastOrientation = {0.f, 0.f, 0.f, 0.f};
    float lastPosition[3] = {0.f, 0.f, 0.f};

    float currentMagnetometerValues[4] = {0.f, 0.f, 0.f, 0.f};

    int screenWidth = 0;
    int screenHeight = 0;
    int screenRotation = 0;

    bool renderingParamsChanged = true;
    bool glContextRecreated = false;

    bool running = false;
    bool streaming = false;
    std::thread inputThread;

    // Une one texture per eye, no need for swapchains.
    GLuint lobbyTextures[2] = {0, 0};
    GLuint streamTextures[2] = {0, 0};

    float eyeOffsets[2] = {0.0, 0.0};
    AlvrFov fovArr[2] = {};
    AlvrViewParams viewParams[2] = {};
    AlvrDeviceMotion deviceMotion = {};

    NativeContext() {
        memset(&fovArr, 0, (sizeof(fovArr)) / sizeof(int));
        memset(&viewParams, 0, (sizeof(viewParams)) / sizeof(int));
        memset(&deviceMotion, 0, (sizeof(deviceMotion)) / sizeof(int));
    }
};

NativeContext CTX = {};

int64_t GetBootTimeNano() {
    struct timespec res = {};
    clock_gettime(CLOCK_BOOTTIME, &res);
    return (res.tv_sec * 1e9) + res.tv_nsec;
}

// Inverse unit quaternion
AlvrQuat inverseQuat(AlvrQuat q) { return {-q.x, -q.y, -q.z, q.w}; }

void cross(float a[3], float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void quatVecMultiply(AlvrQuat q, float v[3], float out[3]) {
    float rv[3], rrv[3];
    float r[3] = {q.x, q.y, q.z};
    cross(r, v, rv);
    cross(r, rv, rrv);
    for (int i = 0; i < 3; i++) {
        out[i] = v[i] + 2 * (q.w * rv[i] + rrv[i]);
    }
}

void offsetPosWithQuat(AlvrQuat q, float offset[3], float outPos[3]) {
    float rotatedOffset[3];
    quatVecMultiply(q, offset, rotatedOffset);

    outPos[0] -= rotatedOffset[0];
    outPos[1] -= rotatedOffset[1] - FLOOR_HEIGHT;
    outPos[2] -= rotatedOffset[2];
}

AlvrFov getFov(CardboardEye eye) {
    float f[4];
    CardboardLensDistortion_getFieldOfView(CTX.lensDistortion, eye, f);

    AlvrFov fov = {};
    fov.left = -f[0];
    fov.right = f[1];
    fov.up = f[3];
    fov.down = -f[2];

    return fov;
}

AlvrPose getPose(uint64_t timestampNs) {
    AlvrPose pose = {};
    bool returnLastPosition = false;

    if (!CTX.arcoreEnabled || (CTX.arcoreEnabled && !useARCoreOrientation)) {
        float pos[3];
        float q[4];
        CardboardHeadTracker_getPose(CTX.headTracker, (int64_t) timestampNs, kLandscapeLeft, pos, q);

        auto inverseOrientation = AlvrQuat{q[0], q[1], q[2], q[3]};
        pose.orientation = inverseQuat(inverseOrientation);
        CTX.lastOrientation = pose.orientation;
    }

    if (CTX.arcoreEnabled && CTX.arSession != nullptr) {
        if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
            throw std::runtime_error("Failed to get EGL context in getPose.");
            returnLastPosition = false;
            goto out;
        }

        int ret = ArSession_update(CTX.arSession, CTX.arFrame);
        if (ret != AR_SUCCESS) {
            error("getPose: ArSession_update failed (%d), using last position", ret);
            returnLastPosition = true;
            goto out;
        }

        ArCamera *arCamera = nullptr;
        ArFrame_acquireCamera(CTX.arSession, CTX.arFrame, &arCamera);

        ArTrackingState arTrackingState;
        ArCamera_getTrackingState(CTX.arSession, arCamera, &arTrackingState);
        if (arTrackingState != AR_TRACKING_STATE_TRACKING) {
            error("getPose: Camera is not tracking, using last position");
            returnLastPosition = true;
            ArCamera_release(arCamera);
            goto out;
        }

        ArPose *arPose = nullptr;
        ArPose_create(CTX.arSession, nullptr, &arPose);
        ArCamera_getDisplayOrientedPose(CTX.arSession, arCamera, arPose);
        // ArPose_getPoseRaw() returns a pose in {qx, qy, qz, qw, tx, ty, tz} format.
        float arRawPose[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
        ArPose_getPoseRaw(CTX.arSession, arPose, arRawPose);


        /* We use an anchor here for two things:
         *
         * 1. To determine floor position by finding the lowest detected plane. We do this by
         * placing an anchor in the center position of the plane, and if it's lower than the
         * currently placed anchor (CTX.floorAnchor), we replace it.
         *
         * (By default, ARCore's "world coordinates" space begins wherever the device is, but this
         * can desync over time. Anchor position adapts to world space movement.)
         *
         * ~~2. To determine the rotation of world space. This allows us to improve tracking.~~
         * nevermind, that doesn't seem to work, orientation is stuck :(
         */
        ArTrackableList *trackables = nullptr;
        ArTrackableList_create(CTX.arSession, &trackables);
        ArFrame_getUpdatedTrackables(CTX.arSession, CTX.arFrame, AR_TRACKABLE_PLANE, trackables);
        int32_t detectedPlaneCount;
        ArTrackableList_getSize(CTX.arSession, trackables, &detectedPlaneCount);

        for (int i = 0; i < detectedPlaneCount; i++) {
            ArTrackable* arTrackable = nullptr;
            ArTrackableList_acquireItem(CTX.arSession, trackables, i,
                                        &arTrackable);
            const ArPlane *plane = ArAsPlane(arTrackable);
            ArTrackingState planeTrackingState;
            ArTrackable_getTrackingState(CTX.arSession, arTrackable, &planeTrackingState);
            ArPlaneType planeType;
            ArPlane_getType(CTX.arSession, plane, &planeType);
            if (planeTrackingState == AR_TRACKING_STATE_TRACKING &&
                planeType == AR_PLANE_HORIZONTAL_UPWARD_FACING) {
                ArPose *planePose = nullptr;
                ArPose_create(CTX.arSession, nullptr, &planePose);
                ArPlane_getCenterPose(CTX.arSession, plane, planePose);

                bool reanchor = false;
                if (CTX.arFloorAnchor == nullptr) {
                    reanchor = true;
                } else {
                    ArPose *currentFloorPose = nullptr;
                    ArPose_create(CTX.arSession, nullptr, &currentFloorPose);

                    ArAnchor_getPose(CTX.arSession, CTX.arFloorAnchor, currentFloorPose);
                    float currentFloorPoseRaw[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
                    ArPose_getPoseRaw(CTX.arSession, currentFloorPose, currentFloorPoseRaw);

                    float planePoseRaw[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
                    ArPose_getPoseRaw(CTX.arSession, planePose, planePoseRaw);

                    if (planePoseRaw[5] < currentFloorPoseRaw[5]) {
                        info("Found new plane lower than pose (current %f vs new %f), reanchoring",
                             currentFloorPoseRaw[5], planePoseRaw[5]);
                        reanchor = true;
                    }
                }

                if (reanchor) {
                    if (CTX.arFloorAnchor != nullptr) {
                        ArAnchor_detach(CTX.arSession, CTX.arFloorAnchor);
                        ArAnchor_release(CTX.arFloorAnchor);
                    }

                    ArPose *planePoseNoRotation = ArPose_extractTranslation(CTX.arSession,
                                                                            planePose);
                    ArTrackable_acquireNewAnchor(CTX.arSession, arTrackable, planePoseNoRotation,
                                                 &CTX.arFloorAnchor);
                    ArPose_destroy(planePoseNoRotation);
                }

                ArPose_destroy(planePose);
            }
        }

        ArTrackableList_destroy(trackables);

        float anchorRawPose[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
        if (CTX.arFloorAnchor != nullptr) {
            ArPose *anchorPose = nullptr;
            ArPose_create(CTX.arSession, nullptr, &anchorPose);
            ArAnchor_getPose(CTX.arSession, CTX.arFloorAnchor, anchorPose);
            ArPose_getPoseRaw(CTX.arSession, anchorPose, anchorRawPose);
            info("anchor pose %f %f %f %f %f %f %f", anchorRawPose[0], anchorRawPose[1], anchorRawPose[2], anchorRawPose[3], anchorRawPose[4], anchorRawPose[5], anchorRawPose[6]);
        }


        pose.position[0] = arRawPose[4];
        pose.position[1] = arRawPose[5] - anchorRawPose[5];
        pose.position[2] = arRawPose[6];

        for (int i = 0; i < 3; i++) {
            CTX.lastPosition[i] = arRawPose[i + 4];
        }

        if (useARCoreOrientation) {
            auto orientation = AlvrQuat{arRawPose[0], arRawPose[1], arRawPose[2],
                                               arRawPose[3]};
            pose.orientation = orientation;
            CTX.lastOrientation = pose.orientation;
        }

        ArPose_destroy(arPose);
        ArCamera_release(arCamera);
    }

out:
    if (returnLastPosition) {
        pose.orientation = CTX.lastOrientation;
        for (int i = 0; i < 3; i++) {
            pose.position[i] = CTX.lastPosition[i];
        }
    }

    return pose;
}

void updateViewConfigs(uint64_t targetTimestampNs = 0) {
    if (!targetTimestampNs)
        targetTimestampNs = GetBootTimeNano() + alvr_get_head_prediction_offset_ns();

    AlvrPose headPose = getPose(targetTimestampNs);

    CTX.deviceMotion.device_id = HEAD_ID;
    CTX.deviceMotion.pose = headPose;

    float headToEye[3] = {CTX.eyeOffsets[kLeft], 0.0, 0.0};

    CTX.viewParams[kLeft].pose = headPose;
    offsetPosWithQuat(headPose.orientation, headToEye, CTX.viewParams[kLeft].pose.position);
    CTX.viewParams[kLeft].fov = CTX.fovArr[kLeft];

    headToEye[0] = CTX.eyeOffsets[kRight];
    CTX.viewParams[kRight].pose = headPose;
    offsetPosWithQuat(headPose.orientation, headToEye, CTX.viewParams[kRight].pose.position);
    CTX.viewParams[kRight].fov = CTX.fovArr[kRight];
}

void inputThread() {
    auto deadline = std::chrono::steady_clock::now();

    if (CTX.arcoreEnabled) {
        /* ARCore requires an EGL context to work. Since we're calling it from a secondary
         * thread that is not the main GL thread, we need to provide our own context. */
        info("inputThread: creating ARCore EGL context for input thread");
        // 1. Initialize EGL
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            throw std::runtime_error("Failed to get EGL display.");
        }

        if (!eglInitialize(display, nullptr, nullptr)) {
            throw std::runtime_error("Failed to initialize EGL.");
        }

        // 2. Choose EGL configuration
        EGLint numConfigs;
        EGLConfig config;
        EGLint configAttribs[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_NONE
        };

        if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
            throw std::runtime_error("Failed to choose EGL config.");
        }

        if (numConfigs == 0) {
            throw std::runtime_error("No suitable EGL configurations found.");
        }

        // 3. Create an offscreen (pbuffer) surface
        EGLint pbufferAttribs[] = {
                EGL_WIDTH, 1920,
                EGL_HEIGHT, 1920,
                EGL_NONE
        };

        EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
        if (surface == EGL_NO_SURFACE) {
            throw std::runtime_error("Failed to create EGL pbuffer surface.");
        }

        // 4. Create an EGL context
        EGLint contextAttribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 3,  // OpenGL ES 3.0 context
                EGL_NONE
        };

        EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (context == EGL_NO_CONTEXT) {
            throw std::runtime_error("Failed to create EGL context.");
        }

        // 5. Bind the context to the current thread
        if (!eglMakeCurrent(display, surface, surface, context)) {
            throw std::runtime_error("Failed to make EGL context current.");
        }
    }

    info("inputThread: thread staring...");
    while (CTX.streaming) {

        auto targetTimestampNs = GetBootTimeNano() + alvr_get_head_prediction_offset_ns();
        updateViewConfigs(targetTimestampNs);

        alvr_send_tracking(
            targetTimestampNs, CTX.viewParams, &CTX.deviceMotion, 1, nullptr, nullptr);

        deadline += std::chrono::nanoseconds((uint64_t) (1e9 / 60.f / 3));
        std::this_thread::sleep_until(deadline);
    }
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
    CTX.javaVm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_initializeNative(
    JNIEnv *env, jobject obj, jint screenWidth, jint screenHeight, jfloat refreshRate, jboolean enableARCore) {
    CTX.javaContext = env->NewGlobalRef(obj);

    uint32_t viewWidth = std::max(screenWidth, screenHeight) / 2;
    uint32_t viewHeight = std::min(screenWidth, screenHeight);

    alvr_initialize_android_context((void *) CTX.javaVm, (void *) CTX.javaContext);

    float refreshRatesBuffer[1] = {refreshRate};

    AlvrClientCapabilities caps = {};
    caps.default_view_height = viewHeight;
    caps.default_view_width = viewWidth;
    caps.external_decoder = false;
    caps.refresh_rates = refreshRatesBuffer;
    caps.refresh_rates_count = 1;
    caps.foveated_encoding =
        true;   // By default disable FFE (can be force-enabled by Server Settings
    caps.encoder_high_profile = true;
    caps.encoder_10_bits = true;
    caps.encoder_av1 = true;

    alvr_initialize(caps);

    Cardboard_initializeAndroid(CTX.javaVm, CTX.javaContext);
    CTX.headTracker = CardboardHeadTracker_create();

    CTX.arcoreEnabled = (bool) enableARCore;
    if (CTX.arcoreEnabled) {
        if (ArSession_create(env, CTX.javaContext, &CTX.arSession) != AR_SUCCESS) {
            error("initializeNative: Could not create ARCore session");
            CTX.arcoreEnabled = false;
            return;
        }

        ArConfig* arConfig = nullptr;
        ArConfig_create(CTX.arSession, &arConfig);

        // Explicitly disable all unnecessary features to preserve CPU power.
        ArConfig_setDepthMode(CTX.arSession, arConfig, AR_DEPTH_MODE_DISABLED);
        ArConfig_setLightEstimationMode(CTX.arSession, arConfig, AR_LIGHT_ESTIMATION_MODE_DISABLED);
        ArConfig_setPlaneFindingMode(CTX.arSession, arConfig, AR_PLANE_FINDING_MODE_HORIZONTAL_AND_VERTICAL);
        ArConfig_setCloudAnchorMode(CTX.arSession, arConfig, AR_CLOUD_ANCHOR_MODE_DISABLED);

        // Set "latest camera image" update mode (ArSession_update returns immediately without blocking)
        ArConfig_setUpdateMode(CTX.arSession, arConfig, AR_UPDATE_MODE_LATEST_CAMERA_IMAGE);

        // TODO: Add camera config filter:
        // https://developers.google.com/ar/develop/c/camera-configs

        if (ArSession_configure(CTX.arSession, arConfig) != AR_SUCCESS) {
            error("initializeNative: Could not configure ARCore session");
            return;
        }

        ArFrame_create(CTX.arSession, &CTX.arFrame);
    }
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_destroyNative(JNIEnv *,
                                                                                        jobject) {
    alvr_destroy_opengl();
    alvr_destroy();

    CardboardHeadTracker_destroy(CTX.headTracker);
    CTX.headTracker = nullptr;
    CardboardLensDistortion_destroy(CTX.lensDistortion);
    CTX.lensDistortion = nullptr;
    CardboardDistortionRenderer_destroy(CTX.distortionRenderer);
    CTX.distortionRenderer = nullptr;

    if (CTX.arcoreEnabled && CTX.arSession != nullptr) {
        ArSession_destroy(CTX.arSession);
        CTX.arSession = nullptr;

        ArFrame_destroy(CTX.arFrame);
        CTX.arFrame = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_resumeNative(JNIEnv *,
                                                                                       jobject) {
    CardboardHeadTracker_resume(CTX.headTracker);
    if (CTX.arcoreEnabled && CTX.arSession != nullptr) {
        ArSession_resume(CTX.arSession);
    }

    CTX.renderingParamsChanged = true;

    uint8_t *buffer;
    int size;
    CardboardQrCode_getSavedDeviceParams(&buffer, &size);
    if (size == 0) {
        CardboardQrCode_scanQrCodeAndSaveDeviceParams();
    }
    CardboardQrCode_destroy(buffer);

    CTX.running = true;

    alvr_resume();
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_pauseNative(JNIEnv *,
                                                                                      jobject) {
    alvr_pause();

    if (CTX.running) {
        CTX.running = false;
    }

    CardboardHeadTracker_pause(CTX.headTracker);
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_surfaceCreatedNative(JNIEnv *, jobject) {
    alvr_initialize_opengl();

    CTX.glContextRecreated = true;
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_setScreenResolutionNative(
    JNIEnv *, jobject, jint width, jint height) {
    CTX.screenWidth = width;
    CTX.screenHeight = height;

    CTX.renderingParamsChanged = true;
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_setScreenRotationNative(
    JNIEnv *, jobject, jint rotation) {
    CTX.screenRotation = rotation;
    CTX.renderingParamsChanged = true;
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_sendBatteryLevel(
    JNIEnv *, jobject, jfloat level, jboolean plugged) {
    alvr_send_battery(HEAD_ID, level, plugged);
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_renderNative(JNIEnv *,
                                                                                       jobject) {
    try {
        if (CTX.renderingParamsChanged) {
            info("renderingParamsChanged, processing new params");
            uint8_t *buffer;
            int size;
            CardboardQrCode_getSavedDeviceParams(&buffer, &size);

            if (size == 0) {
                return;
            }

            info("renderingParamsChanged, sending new params to alvr");
            if (CTX.lensDistortion) {
                CardboardLensDistortion_destroy(CTX.lensDistortion);
                CTX.lensDistortion = nullptr;
            }
            info("renderingParamsChanged, destroyed distortion");
            CTX.lensDistortion =
                CardboardLensDistortion_create(buffer, size, CTX.screenWidth, CTX.screenHeight);

            CardboardQrCode_destroy(buffer);
            *buffer = 0;

            if (CTX.distortionRenderer) {
                CardboardDistortionRenderer_destroy(CTX.distortionRenderer);
                CTX.distortionRenderer = nullptr;
            }
            const CardboardOpenGlEsDistortionRendererConfig config{kGlTexture2D};
            CTX.distortionRenderer = CardboardOpenGlEs2DistortionRenderer_create(&config);

            for (int eye = 0; eye < 2; eye++) {
                CardboardMesh mesh;
                CardboardLensDistortion_getDistortionMesh(
                    CTX.lensDistortion, (CardboardEye) eye, &mesh);
                CardboardDistortionRenderer_setMesh(
                    CTX.distortionRenderer, &mesh, (CardboardEye) eye);

                float matrix[16] = {};
                CardboardLensDistortion_getEyeFromHeadMatrix(
                    CTX.lensDistortion, (CardboardEye) eye, matrix);
                CTX.eyeOffsets[eye] = matrix[12];
            }

            CTX.fovArr[kLeft] = getFov(kLeft);
            CTX.fovArr[kRight] = getFov(kRight);

            if (CTX.arcoreEnabled && CTX.arSession != nullptr) {
                ArSession_setDisplayGeometry(
                    CTX.arSession, CTX.screenRotation, CTX.screenWidth, CTX.screenHeight);
            }

            info("renderingParamsChanged, updating new view configs (FOV) to alvr");
            // alvr_send_views_config(fovArr, CTX.eyeOffsets[0] - CTX.eyeOffsets[1]);
        }

        // Note: if GL context is recreated, old resources are already freed.
        if (CTX.renderingParamsChanged && !CTX.glContextRecreated) {
            info("Pausing ALVR since glContext is not recreated, deleting textures");
            alvr_pause_opengl();

            GL(glDeleteTextures(2, CTX.lobbyTextures));
        }

        if (CTX.renderingParamsChanged || CTX.glContextRecreated) {
            info("Rebuilding, binding textures, Resuming ALVR since glContextRecreated %b, "
                 "renderingParamsChanged %b",
                 CTX.renderingParamsChanged,
                 CTX.glContextRecreated);
            GL(glGenTextures(2, CTX.lobbyTextures));

            for (auto &lobbyTexture : CTX.lobbyTextures) {
                GL(glBindTexture(GL_TEXTURE_2D, lobbyTexture));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
                GL(glTexImage2D(GL_TEXTURE_2D,
                                0,
                                GL_RGB,
                                CTX.screenWidth / 2,
                                CTX.screenHeight,
                                0,
                                GL_RGB,
                                GL_UNSIGNED_BYTE,
                                nullptr));
            }

            const uint32_t *targetViews[2] = {(uint32_t *) &CTX.lobbyTextures[0],
                                              (uint32_t *) &CTX.lobbyTextures[1]};
            alvr_resume_opengl(CTX.screenWidth / 2, CTX.screenHeight, targetViews, 1, true);

            if (CTX.arcoreEnabled && CTX.arSession != nullptr) {
                GLuint arTextureIdArray[1];
                glGenTextures(1, arTextureIdArray);
                CTX.arTexture = arTextureIdArray[0];

                GL(glBindTexture(GL_TEXTURE_2D, CTX.arTexture));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

                ArSession_setCameraTextureName(CTX.arSession, CTX.arTexture);
            }

            CTX.renderingParamsChanged = false;
            CTX.glContextRecreated = false;
        }

        AlvrEvent event;
        while (alvr_poll_event(&event)) {
            if (event.tag == ALVR_EVENT_HUD_MESSAGE_UPDATED) {
                auto message_length = alvr_hud_message(nullptr);
                auto message_buffer = std::vector<char>(message_length);

                alvr_hud_message(&message_buffer[0]);
                info("ALVR Poll Event: HUD Message Update - %s", &message_buffer[0]);

                if (message_length > 0)
                    alvr_update_hud_message_opengl(&message_buffer[0]);
            }
            if (event.tag == ALVR_EVENT_STREAMING_STARTED) {
                info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, generating and binding "
                     "textures...");
                auto config = event.STREAMING_STARTED;

                auto settings_len = alvr_get_settings_json(nullptr);
                auto settings_buffer = std::vector<char>(settings_len);
                alvr_get_settings_json(&settings_buffer[0]);

                info("Got settings from ALVR Server - %s", &settings_buffer[0]);
                if (settings_len > 900)   // to workthough logcat buffer limit
                    info("Got settings from ALVR Server - %s", &settings_buffer[900]);
                json settings_json = json::parse(&settings_buffer[0]);

                GL(glGenTextures(2, CTX.streamTextures));

                for (auto &streamTexture : CTX.streamTextures) {
                    GL(glBindTexture(GL_TEXTURE_2D, streamTexture));
                    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
                    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
                    GL(glTexImage2D(GL_TEXTURE_2D,
                                    0,
                                    GL_RGB,
                                    config.view_width,
                                    config.view_height,
                                    0,
                                    GL_RGB,
                                    GL_UNSIGNED_BYTE,
                                    nullptr));
                }

                CTX.fovArr[0] = getFov((CardboardEye) 0);
                CTX.fovArr[1] = getFov((CardboardEye) 1);

                info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, View configs updated...");

                auto leftIntHandle = (uint32_t) CTX.streamTextures[0];
                auto rightIntHandle = (uint32_t) CTX.streamTextures[1];
                const uint32_t *textureHandles[2] = {&leftIntHandle, &rightIntHandle};

                auto render_config = AlvrStreamConfig{};
                render_config.view_resolution_width = config.view_width;
                render_config.view_resolution_height = config.view_height;
                render_config.swapchain_textures = textureHandles;
                render_config.swapchain_length = 1;

                render_config.enable_foveation = false;
                if (!settings_json["video"].is_null()) {
                    if (!settings_json["video"]["foveated_encoding"].is_null()) {
                        info("settings_json.video.foveated_encoding is %s",
                             settings_json["video"]["foveated_encoding"].dump().c_str());

                        // Foveated encoding would be a "Enabled": {Array} or "Disabled" String
                        if (!settings_json["video"]["foveated_encoding"].is_string()) {
                            render_config.enable_foveation = true;
                            render_config.foveation_center_size_x =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["center_size_x"];
                            render_config.foveation_center_size_y =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["center_size_y"];
                            render_config.foveation_center_shift_x =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["center_shift_x"];
                            render_config.foveation_center_shift_y =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["center_shift_y"];
                            render_config.foveation_edge_ratio_x =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["edge_ratio_x"];
                            render_config.foveation_edge_ratio_y =
                                settings_json["video"]["foveated_encoding"]["Enabled"]
                                             ["edge_ratio_y"];
                        } else
                            info("foveated_encoding is Disabled");
                    } else
                        error("settings_json doesn't have a video.foveated_encoding key");
                } else
                    error("settings_json doesn't have a video key");

                info("Settings for foveation:");
                info("render_config.enable_foveation: %b", render_config.enable_foveation);
                info("render_config.foveation_center_size_x: %f",
                     render_config.foveation_center_size_x);
                info("render_config.foveation_center_size_y: %f",
                     render_config.foveation_center_size_y);
                info("render_config.foveation_center_shift_x: %f",
                     render_config.foveation_center_shift_x);
                info("render_config.foveation_center_shift_y: %f",
                     render_config.foveation_center_shift_y);
                info("render_config.foveation_edge_ratio_x: %f",
                     render_config.foveation_edge_ratio_x);
                info("render_config.foveation_edge_ratio_y: %f",
                     render_config.foveation_edge_ratio_y);

                alvr_start_stream_opengl(render_config);

                info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, opengl stream started and "
                     "input "
                     "Thread started...");
                CTX.streaming = true;
                CTX.inputThread = std::thread(inputThread);

            } else if (event.tag == ALVR_EVENT_STREAMING_STOPPED) {
                info("ALVR Poll Event: ALVR_EVENT_STREAMING_STOPPED, Waiting for inputThread to "
                     "join...");
                CTX.streaming = false;
                CTX.inputThread.join();

                GL(glDeleteTextures(2, CTX.streamTextures));
                info("ALVR Poll Event: ALVR_EVENT_STREAMING_STOPPED, Stream stopped deleted "
                     "textures.");
            }
        }

        CardboardEyeTextureDescription viewsDescs[2] = {};
        for (auto &viewsDesc : viewsDescs) {
            viewsDesc.left_u = 0.0;
            viewsDesc.right_u = 1.0;
            viewsDesc.top_v = 1.0;
            viewsDesc.bottom_v = 0.0;
        }

        if (CTX.streaming) {
            void *streamHardwareBuffer = nullptr;

            AlvrViewParams dummyViewParams;
            auto timestampNs = alvr_get_frame(&dummyViewParams, &streamHardwareBuffer);

            if (timestampNs == -1) {
                return;
            }

            uint32_t swapchainIndices[2] = {0, 0};
            alvr_render_stream_opengl(streamHardwareBuffer, swapchainIndices);

            alvr_report_submit(timestampNs, 0);

            viewsDescs[0].texture = CTX.streamTextures[0];
            viewsDescs[1].texture = CTX.streamTextures[1];
        } else {
            AlvrPose pose = getPose(GetBootTimeNano() + VSYNC_QUEUE_INTERVAL_NS);

            AlvrViewInput viewInputs[2] = {};
            for (int eye = 0; eye < 2; eye++) {
                float headToEye[3] = {CTX.eyeOffsets[eye], 0.0, 0.0};
                // offset head pos to Eye Position
                offsetPosWithQuat(pose.orientation, headToEye, viewInputs[eye].pose.position);

                viewInputs[eye].pose = pose;
                viewInputs[eye].fov = getFov((CardboardEye) eye);
                viewInputs[eye].swapchain_index = 0;
            }
            alvr_render_lobby_opengl(viewInputs);

            viewsDescs[0].texture = CTX.lobbyTextures[0];
            viewsDescs[1].texture = CTX.lobbyTextures[1];
        }

        // Note: the Cardboard SDK does not support reprojection!
        // todo: manually implement it?

        // info("nativeRendered: Rendering to Display...");
        CardboardDistortionRenderer_renderEyeToDisplay(CTX.distortionRenderer,
                                                       0,
                                                       0,
                                                       0,
                                                       CTX.screenWidth,
                                                       CTX.screenHeight,
                                                       &viewsDescs[0],
                                                       &viewsDescs[1]);
    } catch (const json::exception &e) {
        error(std::string(std::string(__FUNCTION__) + std::string(__FILE_NAME__) +
                          std::string(e.what()))
                  .c_str());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_switchViewerNative(JNIEnv *, jobject) {
    CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}
