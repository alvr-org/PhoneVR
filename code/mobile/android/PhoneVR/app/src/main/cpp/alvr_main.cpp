#include "alvr_client_core.h"
#include "cardboard.h"
#include <jni.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <deque>
#include <map>
#include <thread>
#include <unistd.h>
#include <vector>

#include "common.h"

void log(AlvrLogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buf[1024];
    int count = vsnprintf(buf, sizeof(buf), format, args);
    if (count > (int) sizeof(buf))
        count = (int) sizeof(buf);
    if (count > 0 && buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    alvr_log(level, buf);

    va_end(args);
}

#define error(...) log(ALVR_LOG_LEVEL_ERROR, __VA_ARGS__)
#define info(...) log(ALVR_LOG_LEVEL_INFO, __VA_ARGS__)
#define debug(...) log(ALVR_LOG_LEVEL_DEBUG, __VA_ARGS__)

uint64_t HEAD_ID = alvr_path_string_to_id("/user/head");

// Note: the Cardboard SDK cannot estimate display time and an heuristic is used instead.
const uint64_t VSYNC_QUEUE_INTERVAL_NS = 50e6;
const float FLOOR_HEIGHT = 1.5;
const int MAXIMUM_TRACKING_FRAMES = 360;

struct Pose {
    float position[3];
    AlvrQuat orientation;
};

struct NativeContext {
    JavaVM *javaVm = jVM;
    jobject javaContext = nullptr;

    CardboardHeadTracker *headTracker = nullptr;
    CardboardLensDistortion *lensDistortion = nullptr;
    CardboardDistortionRenderer *distortionRenderer = nullptr;

    int screenWidth = 0;
    int screenHeight = 0;

    bool renderingParamsChanged = true;
    bool glContextRecreated = false;

    bool running = false;
    bool streaming = false;
    std::thread inputThread;

    // Une one texture per eye, no need for swapchains.
    GLuint lobbyTextures[2] = {};
    GLuint streamTextures[2] = {};

    float eyeOffsets[2] = {};
};

NativeContext CTX = {};

int64_t GetBootTimeNano() {
    struct timespec res = {};
    clock_gettime(CLOCK_BOOTTIME, &res);
    return (res.tv_sec * 1e9) + res.tv_nsec;
}

// Inverse unit quaternion
AlvrQuat inverseQuat(AlvrQuat q) {
    return {-q.x, -q.y, -q.z, q.w};
}

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

Pose getPose(uint64_t timestampNs) {
    Pose pose = {};

    float pos[3];
    float q[4];
    CardboardHeadTracker_getPose(CTX.headTracker, (int64_t) timestampNs, kLandscapeLeft, pos, q);

    auto inverseOrientation = AlvrQuat{q[0], q[1], q[2], q[3]};
    pose.orientation = inverseQuat(inverseOrientation);

    // FIXME: The position is calculated wrong. It behaves correctly when leaning side to side but the overall position is wrong when facing left, right or back.
    // float positionBig[3] = {pos[0] * 5, pos[1] * 5, pos[2] * 5};
    // float headPos[3];
    // quatVecMultiply(pose.orientation, positionBig, headPos);

    pose.position[0] = 0; //-headPos[0];
    pose.position[1] = /*-headPos[1]*/ +FLOOR_HEIGHT;
    pose.position[2] = 0; //-headPos[2];

    debug("returning pos (%f,%f,%f) orient (%f, %f, %f, %f)", pos[0], pos[1], pos[2], q[0], q[1], q[2], q[3]);
    return pose;
}

void inputThread() {
    auto deadline = std::chrono::steady_clock::now();

    info("inputThread: thread staring...");
    while (CTX.streaming) {
        debug("inputThread: streaming...");
        uint64_t targetTimestampNs = GetBootTimeNano() + alvr_get_head_prediction_offset_ns();

        Pose headPose = getPose(targetTimestampNs);

        AlvrDeviceMotion headMotion = {};
        headMotion.device_id = HEAD_ID;
        headMotion.position[0] = headPose.position[0];
        headMotion.position[1] = headPose.position[1];
        headMotion.position[2] = headPose.position[2];
        headMotion.orientation = headPose.orientation;

        alvr_send_tracking(targetTimestampNs, &headMotion, 1/* , nullptr, nullptr */);

        deadline += std::chrono::nanoseconds((uint64_t) (1e9 / 60.f / 3));
        std::this_thread::sleep_until(deadline);
    }
}

//extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
//    CTX.javaVm = vm;
//    return JNI_VERSION_1_6;
//}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_initializeNative(JNIEnv *env, jobject obj,
                                                      jint screenWidth, jint screenHeight) {
    CTX.javaContext = env->NewGlobalRef(obj);

    uint32_t viewWidth = std::max(screenWidth, screenHeight) / 2;
    uint32_t viewHeight = std::min(screenWidth, screenHeight);

    float refreshRatesBuffer[1] = {60.f};

    alvr_initialize((void *) CTX.javaVm, (void *) CTX.javaContext, viewWidth, viewHeight,
                    refreshRatesBuffer, 1, false);

    Cardboard_initializeAndroid(CTX.javaVm, CTX.javaContext);
    CTX.headTracker = CardboardHeadTracker_create();
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_destroyNative(JNIEnv *, jobject) {
    alvr_destroy();

    CardboardHeadTracker_destroy(CTX.headTracker);
    CardboardLensDistortion_destroy(CTX.lensDistortion);
    CardboardDistortionRenderer_destroy(CTX.distortionRenderer);
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_resumeNative(JNIEnv *, jobject) {
    CardboardHeadTracker_resume(CTX.headTracker);

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

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_pauseNative(JNIEnv *, jobject) {
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

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_setScreenResolutionNative(JNIEnv *, jobject, jint width,
                                                               jint height) {
    CTX.screenWidth = width;
    CTX.screenHeight = height;

    CTX.renderingParamsChanged = true;
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_renderNative(JNIEnv *, jobject) {
    if (CTX.renderingParamsChanged) {
        info("renderingParamsChanged, processing new params");
        uint8_t *buffer;
        int size;
        CardboardQrCode_getSavedDeviceParams(&buffer, &size);

        if (size == 0) {
            return;
        }

        info("renderingParamsChanged, sending new params to alvr");
        CardboardLensDistortion_destroy(CTX.lensDistortion);
        CTX.lensDistortion = CardboardLensDistortion_create(buffer, size, CTX.screenWidth,
                                                            CTX.screenHeight);

        CardboardQrCode_destroy(buffer);

        CardboardDistortionRenderer_destroy(CTX.distortionRenderer);
        CTX.distortionRenderer = CardboardOpenGlEs2DistortionRenderer_create();

        for (int eye = 0; eye < 2; eye++) {
            CardboardMesh mesh;
            CardboardLensDistortion_getDistortionMesh(CTX.lensDistortion, (CardboardEye) eye,
                                                      &mesh);
            CardboardDistortionRenderer_setMesh(CTX.distortionRenderer, &mesh, (CardboardEye) eye);

            float matrix[16] = {};
            CardboardLensDistortion_getEyeFromHeadMatrix(CTX.lensDistortion, (CardboardEye) eye,
                                                         matrix);
            CTX.eyeOffsets[eye] = matrix[12];
        }

        AlvrFov fovArr[2] = {getFov((CardboardEye) 0), getFov((CardboardEye) 1)};
        alvr_send_views_config(fovArr, CTX.eyeOffsets[0] - CTX.eyeOffsets[1]);
    }

    // Note: if GL context is recreated, old resources are already freed.
    if (CTX.renderingParamsChanged && !CTX.glContextRecreated) {
        info("Pausing ALVR since glContext is not recreated, deleting textures");
        alvr_pause_opengl();

        glDeleteTextures(2, CTX.lobbyTextures);
    }

    if (CTX.renderingParamsChanged || CTX.glContextRecreated) {
        info("Rebuilding, binding textures, Resuming ALVR since glContextRecreated %b, renderingParamsChanged %b", CTX.renderingParamsChanged, CTX.glContextRecreated);
        glGenTextures(2, CTX.lobbyTextures);

        for (auto &lobbyTexture: CTX.lobbyTextures) {
            glBindTexture(GL_TEXTURE_2D, lobbyTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, CTX.screenWidth / 2, CTX.screenHeight, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, nullptr);
        }

        const uint32_t *targetViews[2] = {(uint32_t *) &CTX.lobbyTextures[0],
                                          (uint32_t *) &CTX.lobbyTextures[1]};
        alvr_resume_opengl(CTX.screenWidth / 2, CTX.screenHeight, targetViews, 1);

        CTX.renderingParamsChanged = false;
        CTX.glContextRecreated = false;
    }

    AlvrEvent event;
    while (alvr_poll_event(&event)) {
        if (event.tag == ALVR_EVENT_HUD_MESSAGE_UPDATED) {
            auto message_length = alvr_hud_message(nullptr);
            auto message_buffer = std::vector<char>(message_length);
            info("ALVR Poll Event: HUD Message Update - %s", &message_buffer[0]);

            alvr_hud_message(&message_buffer[0]);

            alvr_update_hud_message_opengl(&message_buffer[0]);
        }
        if (event.tag == ALVR_EVENT_STREAMING_STARTED) {
            info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, generating and binding textures...");
            auto config = event.STREAMING_STARTED;

            glGenTextures(2, CTX.streamTextures);

            for (auto &streamTexture: CTX.streamTextures) {
                glBindTexture(GL_TEXTURE_2D, streamTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, config.view_width, config.view_height, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            }

            AlvrFov fovArr[2] = {getFov((CardboardEye) 0), getFov((CardboardEye) 1)};
            alvr_send_views_config(fovArr, CTX.eyeOffsets[0] - CTX.eyeOffsets[1]);

            info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, View configs sent...");
            // todo: send battery

            auto leftIntHandle = (uint32_t) CTX.streamTextures[0];
            auto rightIntHandle = (uint32_t) CTX.streamTextures[1];
            const uint32_t *textureHandles[2] = {&leftIntHandle, &rightIntHandle};

            auto render_config = AlvrStreamConfig{};
            render_config.view_resolution_width = config.view_width;
            render_config.view_resolution_height = config.view_height;
            render_config.swapchain_textures = textureHandles;
            render_config.swapchain_length = 1;
            render_config.enable_foveation = config.enable_foveation;
            render_config.foveation_center_size_x = config.foveation_center_shift_x;
            render_config.foveation_center_size_y = config.foveation_center_size_y;
            render_config.foveation_center_shift_x = config.foveation_center_shift_x;
            render_config.foveation_center_shift_y = config.foveation_center_shift_y;
            render_config.foveation_edge_ratio_x = config.foveation_edge_ratio_x;
            render_config.foveation_edge_ratio_y = config.foveation_edge_ratio_y;

            alvr_start_stream_opengl(render_config);

            info("ALVR Poll Event: ALVR_EVENT_STREAMING_STARTED, opengl stream started and input Thread started...");
            CTX.streaming = true;
            CTX.inputThread = std::thread(inputThread);

        } else if (event.tag == ALVR_EVENT_STREAMING_STOPPED) {
            info("ALVR Poll Event: ALVR_EVENT_STREAMING_STOPPED, Waiting for inputThread to join...");
            CTX.streaming = false;
            CTX.inputThread.join();

            glDeleteTextures(2, CTX.streamTextures);
            info("ALVR Poll Event: ALVR_EVENT_STREAMING_STOPPED, Stream stopped deleted textures.");
        }
    }

    CardboardEyeTextureDescription viewsDescs[2] = {};
    for (auto &viewsDesc: viewsDescs) {
        viewsDesc.left_u = 0.0;
        viewsDesc.right_u = 1.0;
        viewsDesc.top_v = 1.0;
        viewsDesc.bottom_v = 0.0;
    }

    if (CTX.streaming) {
        void *streamHardwareBuffer = nullptr;
        auto timestampNs = alvr_get_frame(&streamHardwareBuffer);

        if (timestampNs == -1) {
            return;
        }

        uint32_t swapchainIndices[2] = {0, 0};
        alvr_render_stream_opengl(streamHardwareBuffer, swapchainIndices);

        alvr_report_submit(timestampNs, 0);

        viewsDescs[0].texture = CTX.streamTextures[0];
        viewsDescs[1].texture = CTX.streamTextures[1];
    } else {
        Pose pose = getPose(GetBootTimeNano() + VSYNC_QUEUE_INTERVAL_NS);

        AlvrViewInput viewInputs[2] = {};
        for (int eye = 0; eye < 2; eye++) {
            float headToEye[3] = {CTX.eyeOffsets[eye], 0.0, 0.0};
            float rotatedHeadToEye[3];
            quatVecMultiply(pose.orientation, headToEye, rotatedHeadToEye);

            viewInputs[eye].orientation = pose.orientation;
            viewInputs[eye].position[0] = pose.position[0] - rotatedHeadToEye[0];
            viewInputs[eye].position[1] = pose.position[1] - rotatedHeadToEye[1];
            viewInputs[eye].position[2] = pose.position[2] - rotatedHeadToEye[2];
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
    CardboardDistortionRenderer_renderEyeToDisplay(CTX.distortionRenderer, 0, 0, 0, CTX.screenWidth,
                                                   CTX.screenHeight, &viewsDescs[0],
                                                   &viewsDescs[1]);
}

extern "C" JNIEXPORT void JNICALL
Java_viritualisres_phonevr_ALVRActivity_switchViewerNative(JNIEnv *, jobject) {
    CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}
