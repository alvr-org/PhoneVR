#include <android/native_window_jni.h>
#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <unistd.h>

#include "Eigen"
#include "PVRRenderer.h"
#include "PVRSockets.h"

#include "media/NdkMediaExtractor.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <queue>
#include <sys/stat.h>

#include "common.h"

using namespace std;
using namespace Eigen;
using namespace gvr;
using namespace PVR;

#define JNI_VERS  JNI_VERSION_1_6
#define SUB(func) FUNC(void, func)

JavaVM *jVM;

namespace {
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
    std::thread *mediaThr;
    int64_t vOutPts = -1;
}   // namespace

extern char *ExtDirectory = nullptr;
extern float fpsStreamDecoder;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    // PVR_DB_I("JNI initiating...");
    try {
        jVM = vm;
        JNIEnv *env;
        jint result = jVM->GetEnv((void **) &env, JNI_VERS);
        if (result != JNI_OK) {
            if (result == JNI_EDETACHED) {
                PVR_DB_I("JNI_OnLoad:: JNI Not attached");
            } else if (result == JNI_EVERSION) {
                PVR_DB_I("JNI_OnLoad:: JNI Version not supported");
            } else {
                PVR_DB_I("JNI_OnLoad:: JNI Unknown error");
            }
        }

        PVR_DB_I("JNI_OnLoad::FindClass:: Finding viritualisres/phonevr/Wrap ....");
        jthrowable exc;
        jclass jWrapClass = env->FindClass("viritualisres/phonevr/Wrap");
        if (env->ExceptionCheck()) {
            // jclass jClassFormErr = env->FindClass("java/lang/ClassFormatError");
            exc = env->ExceptionOccurred();
            env->ExceptionClear();

            jboolean isCopy = false;
            jmethodID toString = env->GetMethodID(
                env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
            jstring s = (jstring) (*env).CallObjectMethod(exc, toString);
            const char *utf = (*env).GetStringUTFChars(s, &isCopy);

            PVR_DB_I("JNI_OnLoad:: Exception Caught: " + string(utf));
        }

        if (jWrapClass == NULL) {
            PVR_DB_I("JNI_OnLoad::FindClass:: Cannot Find the class");
        }

        javaWrap = (jclass) env->NewGlobalRef(jWrapClass);
        if (javaWrap == NULL) {
            PVR_DB_I("JNI_OnLoad::GlobalRef:: System ran out of memory");
        } else {
            PVR_DB_I("JNI_OnLoad:: Found.");
        }
    } catch (exception e) {
        PVR_DB_I("JNI_OnLoad::FindClass:: Caught Exception: " + string(e.what()));
    }
    return JNI_VERS;
}

void callJavaMethod(const char *name) {
    PVR_DB_I("JNI callJavaMethod: Calling " + to_string(name));

    try {
        bool isMainThread = true;
        JNIEnv *env;

        jint result = jVM->GetEnv((void **) &env, JNI_VERS);
        if (result != JNI_OK) {
            if (result == JNI_EDETACHED) {
                PVR_DB_I("JNI_callJavaMethod:: JNI Not attached");
            } else if (result == JNI_EVERSION) {
                PVR_DB_I("JNI_callJavaMethod:: JNI Version not supported");
            } else {
                PVR_DB_I("JNI_callJavaMethod:: JNI Unknown error");
            }

            isMainThread = false;
            jint thread_result = jVM->AttachCurrentThread(&env, nullptr);

            if (thread_result != JNI_OK) {
                PVR_DB_I("JNI_callJavaMethod:: Fail to AttachCurrentThread " +
                         to_string(thread_result));
            }
        }

        jthrowable exc;
        jmethodID resultOfMethodID = (env->GetStaticMethodID(javaWrap, name, "()V"));
        if (env->ExceptionCheck()) {
            // jclass jClassFormErr = env->FindClass("java/lang/ClassFormatError");
            exc = env->ExceptionOccurred();
            env->ExceptionClear();

            jboolean isCopy = false;
            jmethodID toString = env->GetMethodID(
                env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
            jstring s = (jstring) (*env).CallObjectMethod(exc, toString);
            const char *utf = (*env).GetStringUTFChars(s, &isCopy);

            PVR_DB_I("JNI_callJavaMethod:: Exception Caught: " + string(utf));
        }

        if (resultOfMethodID == NULL) {
            PVR_DB_I("JNI_callJavaMethod:: Fail to GetStaticMethodID of " + to_string(name));
        }

        env->CallStaticVoidMethod(javaWrap, resultOfMethodID);
        PVR_DB_I("JNI_callJavaMethod:: Success calling " + to_string(name) + "()");

        if (!isMainThread) {
            jint resultOfDetachCurrentThread = jVM->DetachCurrentThread();
            if (resultOfDetachCurrentThread != JNI_OK) {
                PVR_DB_I("JNI_callJavaMethod:: Fail DetachCurrentThread with error code " +
                         to_string(resultOfDetachCurrentThread));
            }
        }
    } catch (exception e) {
        PVR_DB_I("JNI_callJavaMethod::GetStaticMethodID:: Caught Exception: " + string(e.what()));
    }
}

extern "C" void updateJavaTextViewFPS(float f1,
                                      float f2,
                                      float f3,
                                      float cf1,
                                      float cf2,
                                      float cf3,
                                      float cf4,
                                      float cf5,
                                      float ctd1,
                                      float ctd2,
                                      int td1,
                                      int td2) {
    PVR_DB("JNI Calling updataJavaTextViewFPS: " + to_string(f1) + " " + to_string(f2) + " " +
           to_string(f3) + " " + to_string(cf1) + " " + to_string(cf2) + " " + to_string(cf3) +
           " " + to_string(cf4));
    try {
        bool isMainThread = true;
        JNIEnv *env;

        jint result = jVM->GetEnv((void **) &env, JNI_VERS);
        if (result != JNI_OK) {
            if (result == JNI_EDETACHED) {
                PVR_DB("JNI_updataJavaTextViewFPS:: JNI Not attached");
            } else if (result == JNI_EVERSION) {
                PVR_DB("JNI_updataJavaTextViewFPS:: JNI Version not supported");
            } else {
                PVR_DB("JNI_updataJavaTextViewFPS:: JNI Unknown error");
            }
            isMainThread = false;
            jint thread_result = jVM->AttachCurrentThread(&env, nullptr);

            if (thread_result != JNI_OK) {
                PVR_DB("JNI_updataJavaTextViewFPS:: Fail to AttachCurrentThread " +
                       to_string(thread_result));
            }
        }

        jfloat jf1 = f1;
        jfloat jf2 = f2;
        jfloat jf3 = f3;

        jfloat jcf1 = cf1;
        jfloat jcf2 = cf2;
        jfloat jcf3 = cf3;
        jfloat jcf4 = cf4;
        jfloat jcf5 = cf5;

        jfloat jctd1 = ctd1;
        jfloat jctd2 = ctd2;

        jint jtd1 = td1;
        jint jtd2 = td2;

        jthrowable exc;
        jmethodID resultOfMethodID =
            (env->GetStaticMethodID(javaWrap, "updateTextViewFPS", "(FFFFFFFFFFII)V"));
        /*if (env->ExceptionCheck()) {
            //jclass jClassFormErr = env->FindClass("java/lang/ClassFormatError");
            exc = env->ExceptionOccurred();
            env->ExceptionClear();

            jboolean isCopy = false;
            jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString",
                                                  "()Ljava/lang/String;");
            jstring s = (jstring) (*env).CallObjectMethod(exc, toString);
            const char *utf = (*env).GetStringUTFChars(s, &isCopy);

            PVR_DB_I("JNI_callJavaMethod:: Exception Caught: " + string(utf));
        }*/

        if (resultOfMethodID == NULL) {
            PVR_DB("JNI_updataJavaTextViewFPS:: Fail to GetStaticMethodID of updateTextViewFPS");
        }

        env->CallStaticVoidMethod(javaWrap,
                                  resultOfMethodID,
                                  jf1,
                                  jf2,
                                  jf3,
                                  jcf1,
                                  jcf2,
                                  jcf3,
                                  jcf4,
                                  jcf5,
                                  jctd1,
                                  jctd2,
                                  jtd1,
                                  jtd2);
        if (!isMainThread) {
            jint resultOfDetachCurrentThread = jVM->DetachCurrentThread();
            if (resultOfDetachCurrentThread != JNI_OK) {
                PVR_DB("JNI_updataJavaTextViewFPS:: Fail DetachCurrentThread with error code " +
                       to_string(resultOfDetachCurrentThread));
            }
        }
    } catch (exception e) {
        PVR_DB_I("JNI_updataJavaTextViewFPS::GetStaticMethodID:: Caught Exception: " +
                 string(e.what()));
    }
}

SUB(setExtDirectory)(JNIEnv *env, jclass, jstring jExtDir, jint len) {

    try {
        const char *resultOfGetStringUTFChars = env->GetStringUTFChars(jExtDir, nullptr);
        if (resultOfGetStringUTFChars == NULL) {
            PVR_DB_I("JNI_setExtDirectory::GetStringUTFChars:: resultOfGetStringUTFChars is " +
                     to_string(resultOfGetStringUTFChars));
        }

        auto dir = resultOfGetStringUTFChars;
        ExtDirectory = new char[len];
        memcpy(ExtDirectory, dir, len);
        ExtDirectory[len] = '\0';

        env->ReleaseStringUTFChars(jExtDir, dir);

        if (mkdir(string(string(ExtDirectory) + string("/PVR/")).c_str(), 0777)) {
            __android_log_print(ANDROID_LOG_DEBUG,
                                "PVR-JNI-D",
                                "Directory Error %d (%s): %s - %s",
                                errno,
                                string(ExtDirectory).c_str(),
                                string(string(ExtDirectory) + string("/PVR/")).c_str(),
                                strerror(errno));
        }

        PVR_DB_I(
            "--------------------------------------------------------------------------------");
        PVR_DB_I("JNI setExtDirectory: len: " + to_string(len) + ", copdstr: " + ExtDirectory);
    } catch (exception e) {
        PVR_DB_I("JNI_setExtDirectory:: Caught Exception: " + string(e.what()));
    }
}

/////////////////////////////////////// discovery ////////////////////////////////////////////////
SUB(startAnnouncer)(JNIEnv *env, jclass, jstring jIP, jint port) {
    try {
        auto ip = env->GetStringUTFChars(jIP, nullptr);
        if (ip == NULL) {
            PVR_DB_I("JNI_startAnnouncer::GetStringUTFChars:: resultOfGetStringUTFChars is " +
                     to_string(ip));
        }

        PVR_DB_I("JNI startAnnouncer: " + to_string(ip) + ":" + to_string(port));
        PVRStartAnnouncer(
            ip,
            port,
            [] { callJavaMethod("segueToGame"); },
            [](uint8_t *headerBuf, size_t len) {
                vHeader = vector<uint8_t>(headerBuf, headerBuf + len);
            },
            [] { callJavaMethod("unwindToMain"); });
        env->ReleaseStringUTFChars(jIP, ip);
    } catch (exception e) {
        PVR_DB_I("JNI_startAnnouncer:: Caught Exception: " + string(e.what()));
    }
}

SUB(stopAnnouncer)() {
    PVR_DB_I("JNI stopAnnouncer");
    try {
        PVRStopAnnouncer();
    } catch (exception e) {
        PVR_DB_I("JNI_stopAnnouncer:: Caught Exception: " + string(e.what()));
    }
}

////////////////////////////////////// orientation ///////////////////////////////////////////////
SUB(startSendSensorData)(JNIEnv *env, jclass, jint port) {
    PVR_DB_I("JNI startSendSensorData " + to_string(port));

    try {
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
    } catch (exception e) {
        PVR_DB_I("JNI_startSendSensorData:: Caught Exception: " + string(e.what()));
    }
}

SUB(setAccData)(JNIEnv *env, jclass, jfloatArray jarr) {
    // PVR_DB("JNI setAccData");
    try {
        auto *accArr = env->GetFloatArrayElements(jarr, nullptr);
        if (accArr == NULL) {
            PVR_DB("JNI_setAccData:: GetFloatArrayElements returned NULL value ");
        }
        memcpy(newAcc, accArr, 3 * 4);
        env->ReleaseFloatArrayElements(jarr, accArr, JNI_ABORT);
    } catch (exception e) {
        PVR_DB_I("JNI_setAccData:: Caught Exception: " + string(e.what()));
    }
}

///////////////////////////////////// system control & events /////////////////////////////////////
SUB(createRenderer)(JNIEnv *, jclass, jlong jGvrApi) {
    PVR_DB_I("JNI createRenderer");
    try {
        PVRCreateGVR(reinterpret_cast<gvr_context *>(jGvrApi));
    } catch (exception e) {
        PVR_DB_I("JNI_createRenderer:: Caught Exception: " + string(e.what()));
    }
}

SUB(onPause)() {
    PVR_DB_I("JNI onPause");
    try {
        PVRPause();
    } catch (exception e) {
        PVR_DB_I("JNI_onPause:: Caught Exception: " + string(e.what()));
    }
}

SUB(onTriggerEvent)() {
    PVR_DB_I("JNI onTriggerEvent");
    try {
        PVRTrigger();
    } catch (exception e) {
        PVR_DB_I("JNI_onTriggerEvent:: Caught Exception: " + string(e.what()));
    }
}

SUB(onResume)() {
    PVR_DB_I("JNI onResume");
    try {
        PVRResume();
    } catch (exception e) {
        PVR_DB_I("JNI_onResume:: Caught Exception: " + string(e.what()));
    }
}

///////////////////////////////////// video stream & coding //////////////////////////////////////
SUB(setVStreamPort)(JNIEnv *, jclass, jint port) {
    PVR_DB_I("JNI setVStreamPort - " + to_string(port));
    try {
        vPort = (uint16_t) port;
    } catch (exception e) {
        PVR_DB_I("JNI_setVStreamPort:: Caught Exception: " + string(e.what()));
    }
}

SUB(startStream)() {   // cannot pass parameter here or else sigabrit on getting frame (why???)
    PVR_DB_I("JNI startStream");
    try {
        PVRStartReceiveStreams((uint16_t) vPort);
    } catch (exception e) {
        PVR_DB_I("JNI_startStream:: Caught Exception: " + string(e.what()));
    }
}

SUB(startMediaCodec)(JNIEnv *env, jclass, jobject surface) {
    PVR_DB_I("JNI startMediaCodec");
    try {
        window = ANativeWindow_fromSurface(env, surface);

        auto *fmt = AMediaFormat_new();
        AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "video/avc");
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, maxWidth);
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, maxHeight);
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_WIDTH, maxWidth);
        AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_HEIGHT, maxHeight);
        // AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, 62);
        // AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, 1000000);
        AMediaFormat_setBuffer(fmt, "csd-0", &vHeader[0], vHeader.size());

        codec = AMediaCodec_createDecoderByType("video/avc");
        auto m = AMediaCodec_configure(codec, fmt, window, nullptr, 0);

        m = AMediaCodec_start(codec);
        AMediaFormat_delete(fmt);

        PVR_DB_I("JNI MCodec th Setup...");

        mediaThr = new std::thread([] {
            try {
                while (pvrState != PVR_STATE_SHUTDOWN) {
                    if (PVRIsVidBufNeeded())   // emptyVBufs.size() < 3
                    {
                        auto idx = AMediaCodec_dequeueInputBuffer(codec, 1000);   // 1ms timeout
                        if (idx >= 0) {
                            size_t bufSz = 0;
                            uint8_t *buf = AMediaCodec_getInputBuffer(codec, (size_t) idx, &bufSz);
                            PVREnqueueVideoBuf(
                                {buf, static_cast<int>(idx), bufSz});   // into emptyVbuf
                            PVR_DB("[MediaCodec th] Getting MCInputMedia Buf[ " + to_string(bufSz) +
                                   "] @ idx:" + to_string(idx) + ". into emptyVbuf");
                        }
                    }

                    auto fBuf = PVRPopVideoBuf();   // filledVBuf
                    static Clk::time_point oldtime = Clk::now();
                    if (fBuf.idx != -1) {
                        AMediaCodec_queueInputBuffer(codec,
                                                     (size_t) fBuf.idx,
                                                     0,
                                                     fBuf.pktSz,
                                                     fBuf.pts,
                                                     0);   // AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                        PVR_DB("[MediaCodec th] Getting filledVBuf Buf[ " + to_string(fBuf.pktSz) +
                               "] @ idx:" + to_string(fBuf.idx) + ", pts:" + to_string(fBuf.pts) +
                               ". into MCqInputBuf");
                    }

                    AMediaCodecBufferInfo info;
                    auto outIdx = AMediaCodec_dequeueOutputBuffer(codec,
                                                                  &info,
                                                                  1000);   // 1ms timeout
                    if (outIdx >= 0) {
                        PVR_DB(
                            "[MediaCodec th] Output: " +
                            to_string(AMediaFormat_toString(AMediaCodec_getOutputFormat(codec))));

                        /*int result_code = mkdir("/data/data/viritualisres.phonevr/files/", 0777);
                        PVR_DB("[MediaCodec th] Dir path /data/data/viritualisres.phonevr/files/ " +
                        to_string(result_code));

                        auto *file =
                        fopen("/data/data/viritualisres.phonevr/files/mystream.h264","wb");

                        if(file) {
                            auto buf = AMediaCodec_getOutputBuffer(codec, outIdx,
                                                                   reinterpret_cast<size_t
                        *>(&info.size)); fwrite(buf, sizeof(uint8_t), ((info.size)/sizeof(uint8_t)),
                        file); fclose(file); PVR_DB("[MediaCodec th] Wrote stream to file
                        /storage/emulated/0/etc/mystream.h264"); } else { PVR_DB("[MediaCodec th]
                        File open failed /storage/emulated/0/etc/mystream.h264");
                        }*/

                        bool render = info.size != 0;
                        auto status =
                            AMediaCodec_releaseOutputBuffer(codec, (size_t) outIdx, render);
                        if (render)
                            vOutPts = info.presentationTimeUs;

                        fpsStreamDecoder = (1000000000.0 / (Clk::now() - oldtime).count());
                        oldtime = Clk::now();
                        PVR_DB(
                            "[MediaCodec th] MCreleaseOutputBuffer Buf @ idx:" + to_string(outIdx) +
                            ", pts:" +
                            (render ? ("Rendered " + to_string(vOutPts)) : "NotRendered") +
                            ", Status: " + to_string(status) + ", Outsize:" + to_string(info.size));
                    }
                }
            } catch (exception e) {
                PVR_DB_I("JNI_startMediaCodec:: Thread:: Caught Exception: " + string(e.what()));
            }
        });
    } catch (exception e) {
        PVR_DB_I("JNI_startMediaCodec:: Caught Exception: " + string(e.what()));
    }
    pvrState = PVR_STATE_RUNNING;
}

FUNC(jlong, vFrameAvailable)(JNIEnv *) {
    auto pts = vOutPts;
    vOutPts = -1;
    return pts;
}

SUB(stopAll)(JNIEnv *) {
    PVR_DB_I("JNI stopAll");
    try {
        pvrState = PVR_STATE_SHUTDOWN;
        PVRStopStreams();
        PVRDestroyGVR();

        mediaThr->join();
        delete mediaThr;

        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
        ANativeWindow_release(window);
        pvrState = PVR_STATE_IDLE;
    } catch (exception e) {
        PVR_DB_I("JNI_stopAll:: Caught Exception: " + string(e.what()));
    }
}

//////////////////////////////////////////// drawing //////////////////////////////////////////////
FUNC(jint, initSystem)
(JNIEnv *, jclass, jint x, jint y, jint resMul, jfloat offFov, jboolean reproj, jboolean debug) {
    try {
        int w = (x > y ? x : y), h = (x > y ? y : x);
        maxWidth = min(w / 8, resMul * w / h) *
                   8;   // keep pixel aspect ratio (does not influence image aspect ratio)
        maxHeight = min(h / 8, resMul) * 8;
        return PVRInitSystem(maxWidth, maxHeight, offFov, reproj, debug);
    } catch (exception e) {
        PVR_DB_I("JNI_initSystem:: Caught Exception: " + string(e.what()));
    }
}

SUB(drawFrame)(JNIEnv *env, jclass, jlong pts) {
    try {
        PVRRender(pts);
    } catch (exception e) {
        PVR_DB_I("JNI_drawFrame:: Caught Exception: " + string(e.what()));
    }
}
