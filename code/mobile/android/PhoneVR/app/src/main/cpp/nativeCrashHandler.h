#ifndef PHONEVR_NATIVECRASHHANDLER_H
#define PHONEVR_NATIVECRASHHANDLER_H

// 3 methods of backtracing are supported:
// - LIBUNWIND_WITH_REGISTERS_METHOD
// - UNWIND_BACKTRACE_WITH_REGISTERS_METHOD
// - UNWIND_BACKTRACE_WITH_SKIPPING_METHOD
// LIBUNWIND_WITH_REGISTERS_METHOD works more reliably on 32-bit ARM,
// but it is more difficult to build.

// LIBUNWIND_WITH_REGISTERS_METHOD can only be used on 32-bit ARM.
// Android NDK r16b contains "libunwind.a"
// for "armeabi" and "armeabi-v7a" ABIs.
// We can use this library, but we need matching headers,
// namely "libunwind.h" and "__libunwind_config.h".
// For NDK r16b, the headers can be fetched here:
// https://android.googlesource.com/platform/external/libunwind_llvm/+/ndk-r16/include/
#if __arm__
#define LIBUNWIND_WITH_REGISTERS_METHOD 1
#include "libunwind.h"
#endif

// UNWIND_BACKTRACE_WITH_REGISTERS_METHOD can only be used on 32-bit ARM.
#if __arm__
#define UNWIND_BACKTRACE_WITH_REGISTERS_METHOD 1
#endif

// UNWIND_BACKTRACE_WITH_SKIPPING_METHOD be used on any platform.
// It usually does not work on devices with 32-bit ARM CPUs.
// Usually works on devices with 64-bit ARM CPUs in 32-bit mode though.
#define UNWIND_BACKTRACE_WITH_SKIPPING_METHOD 1

#define HIDE_EXPORTS 1
#include <unwind.h>

#define ENABLE_DEMANGLING 1
#if __cplusplus
extern "C"
#endif
char* __cxa_demangle(
        const char* mangled_name,
        char* output_buffer,
        size_t* length,
        int* status);

#include <assert.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <string>

static const size_t address_count_max = 60;

struct BacktraceState {
    // On ARM32 architecture this context is needed
    // for getting backtrace of the before-crash stack,
    // not of the signal handler stack.
    const ucontext_t*   signal_ucontext;

    // On non-ARM32 architectures signal handler stack
    // seems to be "connected" to the before-crash stack,
    // so we only need to skip several initial addresses.
    size_t              address_skip_count;

    size_t              address_count;
    uintptr_t           addresses[address_count_max];

};


std::string StackTraceString;

typedef struct BacktraceState BacktraceState;


void BacktraceState_Init(BacktraceState* state, const ucontext_t* ucontext);

bool BacktraceState_AddAddress(BacktraceState* state, uintptr_t ip);

void LibunwindWithRegisters(BacktraceState* state);

void ProcessRegisters(struct _Unwind_Context* unwind_context, BacktraceState* state);

_Unwind_Reason_Code UnwindBacktraceWithRegistersCallback(struct _Unwind_Context* unwind_context, void* state_voidp);

void UnwindBacktraceWithRegisters(BacktraceState* state);

_Unwind_Reason_Code UnwindBacktraceWithSkippingCallback(struct _Unwind_Context* unwind_context, void* state_voidp);

void UnwindBacktraceWithSkipping(BacktraceState* state);

void PrintBacktrace(BacktraceState* state);

//void SigActionHandler(int sig, siginfo_t* info, void* ucontext);

void SetUpAltStack();

void SetUpSigActionHandler();

#endif //PHONEVR_NATIVECRASHHANDLER_H
