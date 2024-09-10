#include "alvr_client_core.h"
#include "cardboard.h"
#include <GLES3/gl3.h>
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

    int screenWidth = 0;
    int screenHeight = 0;

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

    float pos[3];
    float q[4];
    CardboardHeadTracker_getPose(CTX.headTracker, (int64_t) timestampNs, kLandscapeLeft, pos, q);

    auto inverseOrientation = AlvrQuat{q[0], q[1], q[2], q[3]};
    pose.orientation = inverseQuat(inverseOrientation);

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
    JNIEnv *env, jobject obj, jint screenWidth, jint screenHeight, jfloat refreshRate) {
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
}

extern "C" JNIEXPORT void JNICALL Java_viritualisres_phonevr_ALVRActivity_resumeNative(JNIEnv *,
                                                                                       jobject) {
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

                viewInputs[eye].pose.orientation = pose.orientation;
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
