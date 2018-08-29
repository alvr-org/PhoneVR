#include "Watch.h"
#include <chrono>
#include <unordered_map>

#ifdef __ANDROID__

#include <android/log.h>

#define PRINT(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "LOG_TAG", fmt, ##__VA_ARGS__)
#else
#define PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

using namespace std;
using namespace std::chrono;
typedef high_resolution_clock Clk;

char watchMode = 0;

namespace {
    auto dict = unordered_map<const char *, time_point<Clk>>();
}

void startWatch(const char *const name) {
    auto i = dict.find(name);
    if (i != dict.end())
        i->second = Clk::now();
    else
        dict.insert({name, Clk::now()});
}

void stopWatch(const char *const name) {
    auto i = dict.find(name);
    if (i != dict.end()) {
        float time = (float) duration_cast<milliseconds>(Clk::now() - i->second).count();
        PRINT("%s: %6.3f\n", name, watchMode ? 1 / time : time / 1000);
    }

}

void watchTick(const char *const name) {
    stopWatch(name);
    startWatch(name);
}
