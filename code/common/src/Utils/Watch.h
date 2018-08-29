#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char watchMode;

void startWatch(const char *const name);
void stopWatch(const char *const name);
void watchTick(const char *const name);

#ifdef __cplusplus
}

#include <chrono>
#include <unordered_map>

#ifdef __ANDROID__
#include "../../../../../../AppData/Local/Android/sdk/ndk-bundle/platforms/android-24/arch-x86/usr/include/android/log.h"
#define WPRINT(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "LOG_TAG", fmt, ##__VA_ARGS__)
#else
#define WPRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

using namespace std;

char watchMode = 0;


class Watch
{
    typedef std::chrono::high_resolution_clock WClk;

    const char *name;
    WClk::time_point tp = WClk::now();
    bool mode;

    Watch(const char *const name, bool mode) : name(name), mode(mode) {} // hidden ctor

public:
    static Watch get(const char *const name, bool mode = false);

    void start()
    {
        tp = WClk::now();
    }

    void stop()
    {
        float time = (float) std::chrono::duration_cast<std::chrono::milliseconds>(WClk::now() - tp).count();
        WPRINT("%s: %6.3f\n", name, watchMode ? 1 / time : time / 1000);
    }

    void tick()
    {
        stop();
        start();
    };

};

namespace {
    unordered_map<const char *, Watch> watch_dict;
}

Watch Watch::get(const char *const name, bool mode) //factory
{
    auto i = watch_dict.find(name);
    if (i != watch_dict.end())
        return i->second;
    else {
        Watch w(name, mode);
        watch_dict.insert({name, w});
        return w;
    }
}

void startWatch(const char *const name, bool mode = false) {
    Watch::get(name, mode).start();
}

void stopWatch(const char *const name, bool mode = false) {
    Watch::get(name, mode).stop();
}

void watchTick(const char *const name, bool mode = false) {
    Watch::get(name, mode).tick();
}

#endif // __cplusplus
