#include "NativeErrorHandler.h"

/// Register signal handler for native crashes
static bool registerSignalHandler(CrashSignalHandler handler, struct sigaction old_handlers[NSIG]) {
    struct sigaction sigactionstruct;
    memset(&sigactionstruct, 0, sizeof(sigactionstruct));
    sigactionstruct.sa_flags = SA_SIGINFO;
    sigactionstruct.sa_sigaction = handler;
    // Register new handlers for all signals
    for (int index = 0; index < sizeofa(SIGNALS_TO_CATCH); ++index) {
        const int signo = SIGNALS_TO_CATCH[index];
        if (sigaction(signo, &sigactionstruct, &old_handlers[signo])) {
            return false;
        }
    }
    return true;
}
/// Unregister already register signal handler
static void unregisterSignalHandler(struct sigaction old_handlers[NSIG]) {
    // Recover old handler for all signals
    for (int signo = 0; signo < NSIG; ++signo) {
        const struct sigaction* old_handler = &old_handlers[signo];
        if (!old_handler->sa_handler) {
            continue;
        }
        sigaction(signo, old_handler, nullptr);
    }
}

/// Create a crash message using whatever available such as signal, C++ exception etc
static const char* createCrashMessage(int signo, siginfo* siginfo) {
    void* current_exception = __cxxabiv1::__cxa_current_primary_exception();
    std::type_info* current_exception_type_info = __cxxabiv1::__cxa_current_exception_type();
    size_t buffer_size = 1024;
    char* abort_message = static_cast<char*>(malloc(buffer_size));
    if (current_exception) {
        try {
            // Check if we can get the message
            if (current_exception_type_info) {
                const char* exception_name = current_exception_type_info->name();
                // Try demangling exception name
                int status = -1;
                char demangled_name[buffer_size];
                __cxxabiv1::__cxa_demangle(exception_name, demangled_name, &buffer_size, &status);
                // Check demangle status
                if (status) {
                    // Couldn't demangle, go with exception_name
                    sprintf(abort_message, "Terminating with uncaught exception of type %s", exception_name);
                } else {
                    if (strstr(demangled_name, "nullptr") || strstr(demangled_name, "NULL")) {
                        // Could demangle, go with demangled_name and state that it was null
                        sprintf(abort_message, "Terminating with uncaught exception of type %s", demangled_name);
                    } else {
                        // Could demangle, go with demangled_name and exception.what() if exists
                        try {
                            __cxxabiv1::__cxa_rethrow_primary_exception(current_exception);
                        } catch (std::exception& e) {
                            // Include message from what() in the abort message
                            sprintf(abort_message, "Terminating with uncaught exception of type %s : %s", demangled_name, e.what());
                        } catch (...) {
                            // Just report the exception type since it is not an std::exception
                            sprintf(abort_message, "Terminating with uncaught exception of type %s", demangled_name);
                        }
                    }
                }
                return abort_message;
            } else {
                // Not a cpp exception, assume a custom crash and act like C
            }
        }
        catch (std::bad_cast& bc) {
            // Not a cpp exception, assume a custom crash and act like C
        }
    }
    // Assume C crash and print signal no and code
    sprintf(abort_message, "Terminating with a C crash %d : %d", signo, siginfo->si_code);
    return abort_message;
}

/// Main signal handling function.
static void nativeCrashSignalHandler(int signo, siginfo* siginfo, void* ctxvoid) {
    // Restoring an old handler to make built-in Android crash mechanism work.
    sigaction(signo, &crashInContext->old_handlers[signo], nullptr);
    // Log crash message
    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", createCrashMessage(signo, siginfo));
    // In some cases we need to re-send a signal to run standard bionic handler.
    if (siginfo->si_code <= 0 || signo == SIGABRT) {
        if (syscall(__NR_tgkill, getpid(), gettid(), signo) < 0) {
            _exit(1);
        }
    }
}

/// deinitializeNativeCrashHandler to be called on onPause()
bool deinitializeNativeCrashHandler() {
    // Check if already deinitialized
    if (!crashInContext) return false;
    // Unregister signal handlers
    unregisterSignalHandler(crashInContext->old_handlers);
    // Free singleton crash handler context
    free(crashInContext);
    crashInContext = nullptr;
    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Native crash handler successfully deinitialized.");
    return true;
}
/// like TestFairy.begin() but for crashes
void initializeNativeCrashHandler() {
    // Check if already initialized
    if (crashInContext) {
        __android_log_print(ANDROID_LOG_INFO, "PVR-JNI-EH", "%s", "Native crash handler is already initialized.");
        return;
    }
    // Initialize singleton crash handler context
    crashInContext = static_cast<CrashInContext *>(malloc(sizeof(CrashInContext)));
    memset(crashInContext, 0, sizeof(CrashInContext));
    // Trying to register signal handler.
    if (!registerSignalHandler(&nativeCrashSignalHandler, crashInContext->old_handlers)) {
        deinitializeNativeCrashHandler();
        __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Native crash handler initialization failed.");
        return;
    }
    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Native crash handler successfully initialized.");
}

