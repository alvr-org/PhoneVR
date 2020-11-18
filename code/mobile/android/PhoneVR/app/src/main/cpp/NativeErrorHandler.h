#pragma once

#ifndef PHONEVR_NATIVEERRORHANDLER_H
#define PHONEVR_NATIVEERRORHANDLER_H

// Java
#include <jni.h>
// C++
#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
// C++ ABI
#include <cxxabi.h>
// Android
#include <android/log.h>
#include <unistd.h>

/// Helper macro to get size of an fixed length array during compile time
#define sizeofa(array) sizeof(array) / sizeof(array[0])

/// tgkill syscall id for backward compatibility (more signals available in many linux kernels)
#define __NR_tgkill 270

/// Caught signals
static const int SIGNALS_TO_CATCH[] = {
        SIGABRT,
        SIGBUS,
        SIGFPE,
        SIGSEGV,
        SIGILL,
        SIGSTKFLT,
        SIGTRAP,
};
/// Signal handler context
struct CrashInContext {
    /// Old handlers of signals that we restore on de-initialization. Keep values for all possible
    /// signals, for unused signals nullptr value is stored.
    struct sigaction old_handlers[NSIG];
};
/// Crash handler function signature
typedef void (*CrashSignalHandler)(int, siginfo*, void*);
/// Global instance of context. Since an app can't crash twice in a single run, we can make this singleton.
static CrashInContext* crashInContext = nullptr;

void initializeNativeCrashHandler(void);
bool deinitializeNativeCrashHandler(void);

#endif //PHONEVR_NATIVEERRORHANDLER_H
