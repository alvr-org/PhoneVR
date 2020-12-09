#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <android/native_window_jni.h>
#include <unistd.h>

#include "Eigen"
#include "PVRRenderer.h"
#include "PVRSockets.h"

#include "media/NdkMediaExtractor.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <queue>
#include <sys/stat.h>
#include <android/log.h>
#include <unwind.h>
#include <cxxabi.h>
#include <dlfcn.h>

#include "NativeErrorHandler.h"
#include "nativeCrashHandler.h"



using namespace std;
using namespace Eigen;
using namespace gvr;
using namespace PVR;

#define JNI_VERS JNI_VERSION_1_6
#define FUNC(type, func) extern "C" JNIEXPORT type JNICALL Java_viritualisres_phonevr_Wrap_##func
#define SUB(func) FUNC(void, func)

#define CATCH_CPP_EXCEPTION_AND_THROW_JAVA_EXCEPTION                \
  catch (const std::bad_alloc& e)                                   \
  {                                                               \
    /* OOM exception */                                           \
    jclass jc = env->FindClass("java/lang/OutOfMemoryError");     \
    if(jc) env->ThrowNew (jc, e.what());                          \
  }                                                               \
  catch (const std::ios_base::failure& e)                         \
  {                                                               \
    /* IO exception */                                            \
    jclass jc = env->FindClass("java/io/IOException");            \
    if(jc) env->ThrowNew (jc, e.what());                          \
  }                                                               \
  catch (const std::exception& e)                                 \
  {                                                               \
    /* unknown exception */                                       \
    jclass jc = env->FindClass("java/lang/RuntimeException");     \
    jthrowable exc;                                               \
    exc = env->ExceptionOccurred();                               \
    jboolean isCopy = false;                                      \
    jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");                                       \
    jstring s = (jstring)env->CallObjectMethod(exc, toString);                                       \
    const char* utf = env->GetStringUTFChars(s, &isCopy);                                       \
    PVR_DB(utf)                                       \
    if(jc) env->ThrowNew (jc, e.what());                          \
  }                                                               \
  catch (...)                                                     \
  {                                                               \
    /* Oops I missed identifying this exception! */               \
    jclass jc = env->FindClass("java/lang/Error");                \
    if(jc) env->ThrowNew (jc, "unidentified exception");          \
  }

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
    thread *mediaThr;
    int64_t vOutPts = -1;
}

extern char* ExtDirectory = nullptr;
extern float fpsStreamDecoder;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    //PVR_DB_I("JNI initiating...");
    jVM = vm;
    JNIEnv *env;
    jVM->GetEnv((void **) &env, JNI_VERS);
    javaWrap = (jclass) env->NewGlobalRef(env->FindClass("viritualisres/phonevr/Wrap"));
    return JNI_VERS;
}

void callJavaMethod(const char *name) {
    PVR_DB_I("JNI callJavaMethod: Calling " + to_string(name));
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

extern "C" void updateJavaTextViewFPS(float f1, float f2, float f3, float cf1, float cf2, float cf3)
{
    PVR_DB("JNI Calling updataJavaTextViewFPS: " + to_string(f1) + " " + to_string(f2) + " " + to_string(f3)+ " " + to_string(cf1) + " " + to_string(cf2)+ " " + to_string(cf3));
    bool isMainThread = true;
    JNIEnv *env;
    if (jVM->GetEnv((void **) &env, JNI_VERS) != JNI_OK) {
        isMainThread = false;
        jVM->AttachCurrentThread(&env, nullptr);
    }
    jfloat jf1 = f1;
    jfloat jf2 = f2;
    jfloat jf3 = f3;
    jfloat jcf1 = cf1;
    jfloat jcf2 = cf2;
    jfloat jcf3 = cf3;

    env->CallStaticVoidMethod(javaWrap, env->GetStaticMethodID(javaWrap, "updateTextViewFPS",
                                                               "(FFFFFF)V"), jf1, jf2, jf3, jcf1, jcf2, jcf3);
    if (!isMainThread)
        jVM->DetachCurrentThread();
}

/*
struct android_backtrace_state
{
    void **current;
    void **end;
};

static void dump_stack_latest(int signum, siginfo_t* info, void*ptr) {
    static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};

    int i, f = 0;
    ucontext_t *ucontext = (ucontext_t*)ptr;
    Dl_info dlinfo;
    void **bp = 0;
    void *ip = 0;

    sigsegv_outp("Segmentation Fault!");
    sigsegv_outp("info.si_signo = %d", signum);
    sigsegv_outp("info.si_errno = %d", info->si_errno);
    sigsegv_outp("info.si_code  = %d (%s)", info->si_code, si_codes[info->si_code]);
    sigsegv_outp("info.si_addr  = %p", info->si_addr);
    for(i = 0; i < NGREG; i++)
        sigsegv_outp("reg[%02d]       = 0x" REGFORMAT, i, ucontext->uc_mcontext.gregs[i]);

#ifndef SIGSEGV_NOSTACK
#if defined(SIGSEGV_STACK_IA64) || defined(SIGSEGV_STACK_X86)
#if defined(SIGSEGV_STACK_IA64)
    ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];
    bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
#elif defined(SIGSEGV_STACK_X86)
    ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];
    bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
#endif

    sigsegv_outp("Stack trace:");
    while(bp && ip) {
        if(!dladdr(ip, &dlinfo))
            break;

        const char *symname = dlinfo.dli_sname;

#ifndef NO_CPP_DEMANGLE
        int status;
        char * tmp = __cxa_demangle(symname, NULL, 0, &status);

        if (status == 0 && tmp)
            symname = tmp;
#endif

        sigsegv_outp("% 2d: %p <%s+%lu> (%s)",
                     ++f,
                     ip,
                     symname,
                     (unsigned long)ip - (unsigned long)dlinfo.dli_saddr,
                     dlinfo.dli_fname);

#ifndef NO_CPP_DEMANGLE
        if (tmp)
            free(tmp);
#endif

        if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
            break;

        ip = bp[1];
        bp = (void**)bp[0];
    }
#else
    sigsegv_outp("Stack trace (non-dedicated):");
    sz = backtrace(bt, 20);
    strings = backtrace_symbols(bt, sz);
    for(i = 0; i < sz; ++i)
        sigsegv_outp("%s", strings[i]);
#endif
    sigsegv_outp("End of stack trace.");
#else
    sigsegv_outp("Not printing stack strace.");
#endif
    _exit (-1);
}

_Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context* context,
                                            void* ptr)
{
    /*android_backtrace_state* state = (android_backtrace_state *)arg;
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc)
    {
        if (state->current == state->end)
        {
            return _URC_END_OF_STACK;
        }
        else
        {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;*/

    /*static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};

    int i, f = 0;
    ucontext_t *ucontext = (ucontext_t*)ptr;
    Dl_info dlinfo;
    void **bp = 0;
    void *ip = 0;

    sigsegv_outp("Segmentation Fault!");
    //sigsegv_outp("info.si_signo = %d", signum);
    //sigsegv_outp("info.si_errno = %d", info->si_errno);
    //sigsegv_outp("info.si_code  = %d (%s)", info->si_code, si_codes[info->si_code]);
    //sigsegv_outp("info.si_addr  = %p", info->si_addr);
    //for(i = 0; i < NGREG; i++)
    //    sigsegv_outp("reg[%02d]       = 0x" REGFORMAT, i, ucontext->uc_mcontext.gregs[i]);

#ifndef SIGSEGV_NOSTACK
#if defined(SIGSEGV_STACK_IA64) || defined(SIGSEGV_STACK_X86)
    #if defined(SIGSEGV_STACK_IA64)
    //ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];
    //bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
#elif defined(SIGSEGV_STACK_X86)
    ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];
    bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
#endif

    sigsegv_outp("Stack trace:");
    while(bp && ip) {
        if(!dladdr(ip, &dlinfo))
            break;

        const char *symname = dlinfo.dli_sname;

#ifndef NO_CPP_DEMANGLE
        int status;
        char * tmp = __cxa_demangle(symname, NULL, 0, &status);

        if (status == 0 && tmp)
            symname = tmp;
#endif

        sigsegv_outp("% 2d: %p <%s+%lu> (%s)",
                 ++f,
                 ip,
                 symname,
                 (unsigned long)ip - (unsigned long)dlinfo.dli_saddr,
                 dlinfo.dli_fname);

#ifndef NO_CPP_DEMANGLE
        if (tmp)
            free(tmp);
#endif

        if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
            break;

        ip = bp[1];
        bp = (void**)bp[0];
    }
#else
    sigsegv_outp("Stack trace (non-dedicated):");
    sz = backtrace(bt, 20);
    strings = backtrace_symbols(bt, sz);
    for(i = 0; i < sz; ++i)
        sigsegv_outp("%s", strings[i]);
#endif
    sigsegv_outp("End of stack trace.");
#else
    sigsegv_outp("Not printing stack strace.");
#endif
    _exit (-1);
}

void dump_stack(void)
{
    __android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "android stack dump");

    const int max = 100;
    void* buffer[max];

    android_backtrace_state state;
    state.current = buffer;
    state.end = buffer + max;

    _Unwind_Backtrace(android_unwind_callback, &state);

    int count = (int)(state.current - buffer);

    for (int idx = 0; idx < count; idx++)
    {
        const void* addr = buffer[idx];
        const char* symbol = "";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname)
        {
            symbol = info.dli_sname;
        }
        int status = 0;
        char *demangled = __cxxabiv1::__cxa_demangle(symbol, 0, 0, &status);

        char dummy[150];
        dummy[0] = '\0';
        __android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D",  "%03d- 0x%p %s",idx,
                addr,
                (NULL != demangled && 0 == status) ?
                demangled : symbol );

        //__android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", new std::string(dummy));

        if (NULL != demangled)
            free(demangled);
    }

     __android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "android stack dump done");
}
*/
SUB(setExtDirectory)(JNIEnv *env, jclass, jstring jExtDir, jint len) {

    //try
    //{
        throw std::runtime_error("C++ Meow Moew Madafakas"); // Dont Change this Line -- Catch this error o
        auto dir = env->GetStringUTFChars(jExtDir, nullptr);
        ExtDirectory = new char[len];
        memcpy(ExtDirectory, dir, len);
        ExtDirectory[len] = '\0';

        env->ReleaseStringUTFChars(jExtDir, dir);

        if (mkdir(string(string(ExtDirectory) + string("/PVR/")).c_str(), 0777)) {
            __android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "Directory Error %d (%s): %s - %s",
                                errno,
                                string(ExtDirectory).c_str(),
                                string(string(ExtDirectory) + string("/PVR/")).c_str(),
                                strerror(errno));
        }

        PVR_DB_I(
                "--------------------------------------------------------------------------------");
        PVR_DB_I("JNI setExtDirectory: len: " + to_string(len) + ", copdstr: " + ExtDirectory);
    //}
    //catch (std::runtime_error e)
    //{
        //dump_stack();

        //jclass jc = env->FindClass("java/lang/RuntimeException");
        //if(jc) env->ThrowNew (jc, e.what());
    //}
}


/////////////////////////////////////// discovery ////////////////////////////////////////////////
SUB(startAnnouncer)(JNIEnv *env, jclass, jstring jIP, jint port) {
    auto ip = env->GetStringUTFChars(jIP, nullptr);
    PVR_DB_I("JNI startAnnouncer: "+to_string(ip)+":"+ to_string(port));
    PVRStartAnnouncer(ip, port, [] {
        callJavaMethod("segueToGame");
    }, [](uint8_t *headerBuf, size_t len) {
        vHeader = vector<uint8_t>(headerBuf, headerBuf + len);
    }, [] {
        callJavaMethod("unwindToMain");
    });
    env->ReleaseStringUTFChars(jIP, ip);
}

SUB(stopAnnouncer)() {
    PVR_DB_I("JNI stopAnnouncer");
    PVRStopAnnouncer();
}

////////////////////////////////////// orientation ///////////////////////////////////////////////
SUB(startSendSensorData)(JNIEnv *env, jclass, jint port) {
    PVR_DB_I("JNI startSendSensorData " + to_string(port));

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

SUB(setAccData)(JNIEnv *env, jclass, jfloatArray jarr) {
    //PVR_DB("JNI setAccData");
    auto *accArr = env->GetFloatArrayElements(jarr, nullptr);
    memcpy(newAcc, accArr, 3 * 4);
    env->ReleaseFloatArrayElements(jarr, accArr, JNI_ABORT);
}

///////////////////////////////////// system control & events /////////////////////////////////////
SUB(createRenderer)(JNIEnv *, jclass, jlong jGvrApi) {
    PVR_DB_I("JNI createRenderer");
    PVRCreateGVR(reinterpret_cast<gvr_context *>(jGvrApi));
}

SUB(onPause)() {
    PVR_DB_I("JNI onPause");
    PVRPause();
}

SUB(onTriggerEvent)() {
    PVR_DB_I("JNI onTriggerEvent");
    PVRTrigger();
}

SUB(onResume)() {
    PVR_DB_I("JNI onResume");
    PVRResume();
}

///////////////////////////////////// video stream & coding //////////////////////////////////////
SUB(setVStreamPort)(JNIEnv *, jclass, jint port) {
    PVR_DB_I("JNI setVStreamPort - "+ to_string(port));
    vPort = (uint16_t)port;
}

SUB(startStream)() {// cannot pass parameter here or else sigabrit on getting frame (why???)
    PVR_DB_I("JNI startStream");
    PVRStartReceiveStreams((uint16_t)vPort);
}

SUB(startMediaCodec)(JNIEnv *env, jclass, jobject surface) {
    PVR_DB_I("JNI startMediaCodec");
    window = ANativeWindow_fromSurface(env, surface);


    auto *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, maxWidth);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, maxHeight);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_WIDTH, maxWidth);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_HEIGHT, maxHeight);
    //AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, 62);
    //AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, 1000000);
    AMediaFormat_setBuffer(fmt, "csd-0", &vHeader[0], vHeader.size());

    codec = AMediaCodec_createDecoderByType("video/avc");
    auto m = AMediaCodec_configure(codec, fmt, window, nullptr, 0);

    m = AMediaCodec_start(codec);
    AMediaFormat_delete(fmt);

    PVR_DB_I("JNI MCodec th Setup...");

    mediaThr = new thread([]{
        while (pvrState != PVR_STATE_SHUTDOWN)
        {
            if (PVRIsVidBufNeeded()) //emptyVBufs.size() < 3
            {
                auto idx = AMediaCodec_dequeueInputBuffer(codec, 1000); // 1ms timeout
                if (idx >= 0)
                {
                    size_t bufSz = 0;
                    uint8_t *buf = AMediaCodec_getInputBuffer(codec, (size_t)idx, &bufSz);
                    PVREnqueueVideoBuf({buf, static_cast<int>(idx), bufSz}); // into emptyVbuf
                    PVR_DB("[MediaCodec th] Getting MCInputMedia Buf[ "+ to_string(bufSz)+ "] @ idx:" + to_string(idx) + ". into emptyVbuf");
                }
            }

            auto fBuf = PVRPopVideoBuf(); //filledVBuf
            static Clk::time_point oldtime = Clk::now();
            if (fBuf.idx != -1)
            {
                AMediaCodec_queueInputBuffer(codec, (size_t)fBuf.idx, 0, fBuf.pktSz, fBuf.pts, 0); //AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                PVR_DB("[MediaCodec th] Getting filledVBuf Buf[ "+ to_string(fBuf.pktSz)+ "] @ idx:" + to_string(fBuf.idx) + ", pts:" + to_string(fBuf.pts) +". into MCqInputBuf");
            }

            AMediaCodecBufferInfo info;
            auto outIdx = AMediaCodec_dequeueOutputBuffer(codec, &info, 1000); // 1ms timeout
            if (outIdx >= 0)
            {
                PVR_DB("[MediaCodec th] Output: " + to_string(AMediaFormat_toString(AMediaCodec_getOutputFormat(codec))));

                /*int result_code = mkdir("/data/data/viritualisres.phonevr/files/", 0777);
                PVR_DB("[MediaCodec th] Dir path /data/data/viritualisres.phonevr/files/ " + to_string(result_code));

                auto *file = fopen("/data/data/viritualisres.phonevr/files/mystream.h264","wb");

                if(file) {
                    auto buf = AMediaCodec_getOutputBuffer(codec, outIdx,
                                                           reinterpret_cast<size_t *>(&info.size));
                    fwrite(buf, sizeof(uint8_t), ((info.size)/sizeof(uint8_t)), file);
                    fclose(file);
                    PVR_DB("[MediaCodec th] Wrote stream to file /storage/emulated/0/etc/mystream.h264");
                } else {
                    PVR_DB("[MediaCodec th] File open failed /storage/emulated/0/etc/mystream.h264");
                }*/

                bool render = info.size != 0;
                auto status = AMediaCodec_releaseOutputBuffer(codec, (size_t) outIdx, render);
                if (render)
                    vOutPts = info.presentationTimeUs;

                fpsStreamDecoder = (1000000000.0 / (Clk::now() - oldtime).count());
                oldtime = Clk::now();
                PVR_DB("[MediaCodec th] MCreleaseOutputBuffer Buf @ idx:" + to_string(outIdx) + ", pts:" + (render?("Rendered "+to_string(vOutPts)):"NotRendered") + ", Status: " + to_string(status) + ", Outsize:" + to_string(info.size));
            }
        }
    });
    pvrState = PVR_STATE_RUNNING;
}

FUNC(jlong, vFrameAvailable)(JNIEnv *) {
    auto pts = vOutPts;
    vOutPts = -1;
    return pts;
}

SUB(stopAll)(JNIEnv *) {
    PVR_DB_I("JNI stopAll");
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
FUNC(jint, initSystem)(JNIEnv *, jclass, jint x, jint y, jint resMul, jfloat offFov, jboolean reproj, jboolean debug) {
    int w = (x > y ? x : y), h = (x > y ? y : x);
    maxWidth = min(w / 8, resMul * w / h) * 8; // keep pixel aspect ratio (does not influence image aspect ratio)
    maxHeight = min(h / 8, resMul) * 8;
    return PVRInitSystem(maxWidth, maxHeight, offFov, reproj, debug);
}

SUB(drawFrame)(JNIEnv *env, jclass, jlong pts) {
    PVRRender(pts);
}

// Native Error Handling
SUB(initializeNativeCrashHandler)( JNIEnv* env, jobject /* this */)
{
    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Calling initializeNativeCrashHandler");
    initializeNativeCrashHandler();
}

FUNC(jboolean, deinitializeNativeCrashHandler)( JNIEnv* env, jobject /* this */)
{
    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Calling deinitializeNativeCrashHandler");
    return deinitializeNativeCrashHandler();
}

SUB(crashAndGetExceptionMessage)(JNIEnv *env, jobject thiz) {

    throw std::runtime_error("C++ Meow Moew Madafakas"); // Dont Change this Line -- Catch this error o
}