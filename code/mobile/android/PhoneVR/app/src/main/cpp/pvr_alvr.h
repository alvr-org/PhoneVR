#ifndef PHONEVR_PVR_ALVR_H
#define PHONEVR_PVR_ALVR_H

#include "alvr_client_core.h"
#include "cardboard.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <deque>
#include <jni.h>
#include <map>
#include <thread>
#include <unistd.h>
#include <vector>

#include "common.h"

#define error(...) log(ALVR_LOG_LEVEL_ERROR, __VA_ARGS__)
#define info(...)  log(ALVR_LOG_LEVEL_INFO, __VA_ARGS__)
#define debug(...) log(ALVR_LOG_LEVEL_DEBUG, __VA_ARGS__)

// Note: the Cardboard SDK cannot estimate display time and an heuristic is used instead.
const uint64_t VSYNC_QUEUE_INTERVAL_NS = 50e6;
const float FLOOR_HEIGHT = 1.5;
const int MAXIMUM_TRACKING_FRAMES = 360;

struct Pose {
    float position[3];
    AlvrQuat orientation;
};

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
    GLuint lobbyTextures[2] = {};
    GLuint streamTextures[2] = {};

    float eyeOffsets[2] = {};
};

extern "C" void renderStreamNativePre(void *streamBuffer);

#endif //PHONEVR_PVR_ALVR_H
