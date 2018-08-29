
#include <jni.h>
#include <GLES2/gl2.h>
#include <media/NdkMediaCodec.h>
#include <android/native_window_jni.h>
#include <unistd.h>

#include "Eigen"
#include "PVRRenderer.h"
#include "PVRSockets.h"


#include "media/NdkMediaExtractor.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>


using namespace std;
using namespace Eigen;
using namespace gvr;
using namespace PVR;

#define JNI_VERS JNI_VERSION_1_6
#define FUNC(type, func) extern "C" JNIEXPORT type JNICALL Java_com_rz_phonevr_JavaWrap_##func
#define SUB(func) FUNC(void, func)

namespace {
    JavaVM *jVM;
    jclass javaWrap;

    float newAcc[3];
    uint16_t vPort = 0;


    int maxWidth, maxHeight;
    vector<uint8_t> vHeader;

    array<float, 16> GetMat3(float m[4][4]) {
        array<float, 16> arr;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; ++j)
                arr[i * 3 + j] = m[i][j];
        return arr;
    }

    ANativeWindow *window = nullptr;
    AMediaCodec *codec;
    bool codecRunning;
    thread *mediaThr;
    size_t vBufSz = 0;
    size_t vPktSz = 0;
    int64_t vInpPts = -1;
    int64_t vOutPts = -1;
    uint8_t *vInpBuf;
    mutex vInpMtx;
    mutex codecMtx;


    AMediaExtractor *ex;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    jVM = vm;
    JNIEnv *env;
    jVM->GetEnv((void **) &env, JNI_VERS);
    javaWrap = (jclass) env->NewGlobalRef(env->FindClass("com/rz/phonevr/JavaWrap"));
    return JNI_VERS;
}

void callJavaMethod(const char *name) {
    bool isMainThread = true;
    JNIEnv *env;
    if (jVM->GetEnv((void **) &env, JNI_VERS) != JNI_OK) {
        isMainThread = false;
        jVM->AttachCurrentThread(&env, nullptr);
    }
    env->CallStaticVoidMethod(javaWrap, env->GetStaticMethodID(javaWrap, name, "()V"));
    if (!isMainThread)
        jVM->DetachCurrentThread();
}

/////////////////////////////////////// discovery ////////////////////////////////////////////////
SUB(StartAnnouncer)(JNIEnv *env, jclass, jstring jIP, jint port) {
    auto ip = env->GetStringUTFChars(jIP, nullptr);
    PVRStartAnnouncer(ip, port, [] {
        callJavaMethod("segueToGame");
    }, [](uint8_t *headerBuf, size_t len) {
        vHeader = vector<uint8_t>(headerBuf, headerBuf + len);
    }, [] {
        callJavaMethod("unwindToMain");
    });
    env->ReleaseStringUTFChars(jIP, ip);
}

SUB(StopAnnouncer)() {
    PVRStopAnnouncer();
}

////////////////////////////////////// orientation ///////////////////////////////////////////////
SUB(StartSendSensorData)(JNIEnv *env, jclass, jint port) {
    PVRStartSendSensorData(port, [](float *quat, float *acc) {
        if (gvrApi) {
            auto gmat = gvrApi->GetHeadSpaceFromStartSpaceRotation(GvrApi::GetTimePointNow());
            Quaternionf equat(Map<Matrix3f>(GetMat3(gmat.m).data()));
            quat[0] = equat.w();
            quat[1] = equat.x();
            quat[2] = equat.y();
            quat[3] = equat.z();
            memcpy(acc, newAcc, 3 * 4);
            return true;
        }
        return false;
    });
}

SUB(SetAccData)(JNIEnv *env, jclass, jfloatArray jarr) {
    auto *accArr = env->GetFloatArrayElements(jarr, nullptr);
    memcpy(newAcc, accArr, 3 * 4);
    env->ReleaseFloatArrayElements(jarr, accArr, JNI_ABORT);
}

///////////////////////////////////// system control & events /////////////////////////////////////
SUB(CreateRenderer)(JNIEnv *, jclass, jlong jGvrApi) {
    PVRCreateGVR(reinterpret_cast<gvr_context *>(jGvrApi));
}

SUB(OnPause)() {
    PVRPause();
}

SUB(OnTriggerEvent)() {
    PVRTrigger();
}

SUB(OnResume)() {
    PVRResume();
}

///////////////////////////////////// video stream & coding //////////////////////////////////////
SUB(SetVStreamPort)(JNIEnv *, jclass, jint port) {
    vPort = (uint16_t)port;
}

SUB(StartStream)() {// cannot pass parameter here or else sigabrit on getting frame (why???)
    PVRStartReceiveStreams((uint16_t)vPort, [](uint8_t **buf) {
        if (vBufSz > 0)
            vInpMtx.lock();
        *buf = vInpBuf;
        return vBufSz;

    }, [](size_t sz, int64_t pts, bool eos) {
        vPktSz = sz;
        vInpPts = pts;
        vInpMtx.unlock();
    });
}

SUB(StartMediaCodec)(JNIEnv *env, jclass, jobject surface) {
    window = ANativeWindow_fromSurface(env, surface);

    auto *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, maxWidth);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, maxHeight);
    AMediaFormat_setBuffer(fmt, "csd-0", &vHeader[0], vHeader.size());

    codec = AMediaCodec_createDecoderByType("video/avc");
    auto m = AMediaCodec_configure(codec, fmt, window, nullptr, 0);

    m = AMediaCodec_start(codec);
    AMediaFormat_delete(fmt);

    mediaThr = new thread([]{
        int inpIdx = -1;
        while (pvrState != PVR_STATE_SHUTDOWN) {
            vInpMtx.lock();
            if (inpIdx >= 0 && vPktSz > 0) {
                AMediaCodec_queueInputBuffer(codec, (size_t) inpIdx, 0, vPktSz, (uint64_t) vInpPts, 0); //AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                inpIdx = -1;
            }
            if (inpIdx < 0) {
                inpIdx = AMediaCodec_dequeueInputBuffer(codec, 0); // try 10000
                if (inpIdx >= 0) {
                    vBufSz = 0;
                    vInpBuf = AMediaCodec_getInputBuffer(codec, (size_t)inpIdx, &vBufSz);
                    vPktSz = 0;
                }
            }
            vInpMtx.unlock();

            AMediaCodecBufferInfo info;
            int outIdx = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
            if (outIdx >= 0) {
                bool render = info.size != 0;
                AMediaCodec_releaseOutputBuffer(codec, (size_t) outIdx, render);
                if (render)
                    vOutPts = info.presentationTimeUs;
            }
        }
    });
    pvrState = PVR_STATE_RUNNING;
}

FUNC(jlong, VFrameAvailable)(JNIEnv *) {
    auto pts = vOutPts;
    vOutPts = -1;
    return pts;
}

SUB(StopAll)(JNIEnv *) {
    pvrState = PVR_STATE_SHUTDOWN;
    PVRStopStreams();
    PVRDestroyGVR();

    mediaThr->join();
    delete mediaThr;

    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
    ANativeWindow_release(window);
    pvrState = PVR_STATE_IDLE;
}

//////////////////////////////////////////// drawing //////////////////////////////////////////////
FUNC(jint, InitSystem)(JNIEnv *, jclass, jint x, jint y, jint resMul, jfloat offFov, jfloat addTWarp, jboolean warp, jboolean debug) {
    int w, h;
    if (x > y){
        w = x;
        h = y;
    } else {
        w = y;
        h = x;
    }
    maxWidth = min(w / 8, resMul * w / h) * 8; // keep pixel aspect ratio (does not influence image aspect ratio)
    maxHeight = min(h / 8, resMul) * 8;
    return PVRInitSystem(maxWidth, maxHeight, offFov, addTWarp, warp, debug);
}

SUB(DrawFrame)(JNIEnv *env, jclass, jlong pts) {
    PVRRender(pts);
}

