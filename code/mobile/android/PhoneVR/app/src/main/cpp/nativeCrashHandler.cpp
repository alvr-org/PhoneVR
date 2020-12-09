#include "nativeCrashHandler.h"

void BacktraceState_Init(BacktraceState* state, const ucontext_t* ucontext) {
    assert(state);
    assert(ucontext);
    memset(state, 0, sizeof(BacktraceState));
    state->signal_ucontext = ucontext;
    state->address_skip_count = 3;
}

bool BacktraceState_AddAddress(BacktraceState* state, uintptr_t ip) {
    assert(state);

    // No more space in the storage. Fail.
    if (state->address_count >= address_count_max) {
        __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH",
                            "BackTraceState's Space is full: inAddresses : %d | Max Addresses : %d",
                            state->address_count, address_count_max);

        return false;
    }

#if __thumb__
    // Reset the Thumb bit, if it is set.
    const uintptr_t thumb_bit = 1;
    ip &= ~thumb_bit;
#endif

    if (state->address_count > 0) {
        // Ignore null addresses.
        // They sometimes happen when using _Unwind_Backtrace()
        // with the compiler optimizations,
        // when the Link Register is overwritten by the inner
        // stack frames, like PreCrash() functions in this example.
        if (ip == 0)
            return true;

        // Ignore duplicate addresses.
        // They sometimes happen when using _Unwind_Backtrace()
        // with the compiler optimizations,
        // because we both add the second address from the Link Register
        // in ProcessRegisters() and receive the same address
        // in UnwindBacktraceCallback().
        if (ip == state->addresses[state->address_count - 1])
            return true;
    }

    // Finally add the address to the storage.
    state->addresses[state->address_count++] = ip;
    return true;
}


#if LIBUNWIND_WITH_REGISTERS_METHOD

void LibunwindWithRegisters(BacktraceState* state) {
    assert(state);

    // Initialize unw_context and unw_cursor.
    unw_context_t unw_context = {};
    unw_getcontext(&unw_context);
    unw_cursor_t  unw_cursor = {};
    unw_init_local(&unw_cursor, &unw_context);

    // Get more contexts.
    const ucontext_t* signal_ucontext = state->signal_ucontext;
    assert(signal_ucontext);
    const struct sigcontext* signal_mcontext = &(signal_ucontext->uc_mcontext);
    assert(signal_mcontext);

    // Set registers.
    unw_set_reg(&unw_cursor, UNW_ARM_R0,  signal_mcontext->arm_r0);
    unw_set_reg(&unw_cursor, UNW_ARM_R1,  signal_mcontext->arm_r1);
    unw_set_reg(&unw_cursor, UNW_ARM_R2,  signal_mcontext->arm_r2);
    unw_set_reg(&unw_cursor, UNW_ARM_R3,  signal_mcontext->arm_r3);
    unw_set_reg(&unw_cursor, UNW_ARM_R4,  signal_mcontext->arm_r4);
    unw_set_reg(&unw_cursor, UNW_ARM_R5,  signal_mcontext->arm_r5);
    unw_set_reg(&unw_cursor, UNW_ARM_R6,  signal_mcontext->arm_r6);
    unw_set_reg(&unw_cursor, UNW_ARM_R7,  signal_mcontext->arm_r7);
    unw_set_reg(&unw_cursor, UNW_ARM_R8,  signal_mcontext->arm_r8);
    unw_set_reg(&unw_cursor, UNW_ARM_R9,  signal_mcontext->arm_r9);
    unw_set_reg(&unw_cursor, UNW_ARM_R10, signal_mcontext->arm_r10);
    unw_set_reg(&unw_cursor, UNW_ARM_R11, signal_mcontext->arm_fp);
    unw_set_reg(&unw_cursor, UNW_ARM_R12, signal_mcontext->arm_ip);
    unw_set_reg(&unw_cursor, UNW_ARM_R13, signal_mcontext->arm_sp);
    unw_set_reg(&unw_cursor, UNW_ARM_R14, signal_mcontext->arm_lr);
    unw_set_reg(&unw_cursor, UNW_ARM_R15, signal_mcontext->arm_pc);

    unw_set_reg(&unw_cursor, UNW_REG_IP,  signal_mcontext->arm_pc);
    unw_set_reg(&unw_cursor, UNW_REG_SP,  signal_mcontext->arm_sp);

    // unw_step() does not return the first IP,
    // the address of the instruction which caused the crash.
    // Thus let's add this address manually.
    BacktraceState_AddAddress(state, signal_mcontext->arm_pc);

    //printf("unw_is_signal_frame(): %i\n", unw_is_signal_frame(&unw_cursor));

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&unw_cursor) > 0) {
        unw_word_t ip = 0;
        unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

        bool ok = BacktraceState_AddAddress(state, ip);
        if (!ok)
            break;

        //printf("unw_is_signal_frame(): %i\n", unw_is_signal_frame(&unw_cursor));
    }
}

#endif // #if LIBUNWIND_WITH_REGISTERS_METHOD


#if UNWIND_BACKTRACE_WITH_REGISTERS_METHOD

void ProcessRegisters(
        struct _Unwind_Context* unwind_context, BacktraceState* state) {
    assert(unwind_context);
    assert(state);

    const ucontext_t* signal_ucontext = state->signal_ucontext;
    assert(signal_ucontext);

    const struct sigcontext* signal_mcontext = &(signal_ucontext->uc_mcontext);
    assert(signal_mcontext);

    _Unwind_SetGR(unwind_context, REG_R0,  signal_mcontext->arm_r0);
    _Unwind_SetGR(unwind_context, REG_R1,  signal_mcontext->arm_r1);
    _Unwind_SetGR(unwind_context, REG_R2,  signal_mcontext->arm_r2);
    _Unwind_SetGR(unwind_context, REG_R3,  signal_mcontext->arm_r3);
    _Unwind_SetGR(unwind_context, REG_R4,  signal_mcontext->arm_r4);
    _Unwind_SetGR(unwind_context, REG_R5,  signal_mcontext->arm_r5);
    _Unwind_SetGR(unwind_context, REG_R6,  signal_mcontext->arm_r6);
    _Unwind_SetGR(unwind_context, REG_R7,  signal_mcontext->arm_r7);
    _Unwind_SetGR(unwind_context, REG_R8,  signal_mcontext->arm_r8);
    _Unwind_SetGR(unwind_context, REG_R9,  signal_mcontext->arm_r9);
    _Unwind_SetGR(unwind_context, REG_R10, signal_mcontext->arm_r10);
    _Unwind_SetGR(unwind_context, REG_R11, signal_mcontext->arm_fp);
    _Unwind_SetGR(unwind_context, REG_R12, signal_mcontext->arm_ip);
    _Unwind_SetGR(unwind_context, REG_R13, signal_mcontext->arm_sp);
    _Unwind_SetGR(unwind_context, REG_R14, signal_mcontext->arm_lr);
    _Unwind_SetGR(unwind_context, REG_R15, signal_mcontext->arm_pc);

    // Program Counter register aka Instruction Pointer will contain
    // the address of the instruction where the crash happened.
    // UnwindBacktraceCallback() will not supply us with it.
    BacktraceState_AddAddress(state, signal_mcontext->arm_pc);
}

_Unwind_Reason_Code UnwindBacktraceWithRegistersCallback(
        struct _Unwind_Context* unwind_context, void* state_voidp) {
    assert(unwind_context);
    assert(state_voidp);

    BacktraceState* state = (BacktraceState*)state_voidp;
    assert(state);

    // On the first UnwindBacktraceCallback() call,
    // set registers to _Unwin, d_Context and BacktraceState.
    if (state->address_count == 0) {
        ProcessRegisters(unwind_context, state);
        return _URC_NO_REASON;
    }

    uintptr_t ip = _Unwind_GetIP(unwind_context);
    bool ok = BacktraceState_AddAddress(state, ip);
    if (!ok){
        //__android_log_print( ANDROID_LOG_ERROR, "PVR-JNI-EH","Returnung _URC_END_OF_STACK" );
        return _URC_END_OF_STACK;
    }

    //__android_log_print( ANDROID_LOG_ERROR, "PVR-JNI-EH","Returnung _URC_NO_REASON" );
    return _URC_NO_REASON;
}

void UnwindBacktraceWithRegisters(BacktraceState* state) {
    assert(state);
    _Unwind_Backtrace(UnwindBacktraceWithRegistersCallback, state);
}

#endif // #if UNWIND_BACKTRACE_WITH_REGISTERS_METHOD


#if UNWIND_BACKTRACE_WITH_SKIPPING_METHOD

_Unwind_Reason_Code UnwindBacktraceWithSkippingCallback(
        struct _Unwind_Context* unwind_context, void* state_voidp) {
    assert(unwind_context);
    assert(state_voidp);

    BacktraceState* state = (BacktraceState*)state_voidp;
    assert(state);

    // Skip some initial addresses, because they belong
    // to the signal handler frame.
    if (state->address_skip_count > 0) {
        state->address_skip_count--;
        return _URC_NO_REASON;
    }

    uintptr_t ip = _Unwind_GetIP(unwind_context);
    bool ok = BacktraceState_AddAddress(state, ip);
    if (!ok)
        return _URC_END_OF_STACK;

    return _URC_NO_REASON;
}

void UnwindBacktraceWithSkipping(BacktraceState* state) {
    assert(state);
    _Unwind_Backtrace(UnwindBacktraceWithSkippingCallback, state);
}

#endif // #if UNWIND_BACKTRACE_WITH_SKIPPING_METHOD


void PrintBacktrace(BacktraceState* state) {
    assert(state);

    size_t frame_count = state->address_count;
    for (size_t frame_index = 0; frame_index < frame_count; ++frame_index) {

        void* address = (void*)(state->addresses[frame_index]);
        assert(address);
        /*
        * typedef struct {
                   const char *dli_fname;  /* Pathname of shared object that
                                              contains address
            void       *dli_fbase;  /* Base address at which shared
                                              object is loaded
            const char *dli_sname;  /* Name of symbol whose definition
                                              overlaps addr
            void       *dli_saddr;  /* Exact address of symbol named
                                              in dli_sname
        } Dl_info;
        */
        const char* symbol_name = "";
        const char* symbol_fname = "";
        const void* symbol_fbase = nullptr;
        const void* symbol_saddr = nullptr;

        Dl_info info = {};
        if (dladdr(address, &info) && info.dli_sname) {
            symbol_name = info.dli_sname;
            symbol_fname = info.dli_fname;
            symbol_fbase = info.dli_fbase;
            symbol_saddr = info.dli_saddr;
        }

        // Relative address matches the address which "nm" and "objdump"
        // utilities give you, if you compiled position-independent code
        // (-fPIC, -pie).
        // Android requires position-independent code since Android 5.0.
        unsigned long relative_address = (char*)address - (char*)info.dli_fbase;

        char* demangled = NULL;

#if ENABLE_DEMANGLING
        int status = 0;
        demangled = __cxa_demangle(symbol_name, NULL, NULL, &status);
        if (demangled)
            symbol_name = demangled;
#endif

        assert(symbol_name);
        //__android_log_print( ANDROID_LOG_ERROR, "PVR-JNI-EH","      #%02zu:  0x%lx  (fbase:%p) fname:%s (saddr:%p) sname:%s ", frame_index, relative_address, symbol_fbase, symbol_fname, symbol_saddr, symbol_name);
        //__android_log_print( ANDROID_LOG_ERROR, "PVR-JNI-EH","#%02zu: (fname:%s)--(sname:%s)", frame_index, symbol_fname, symbol_name);
        if( symbol_fname != "" && symbol_name != "" ) {
            /*strcat(message, "\n");
            strcat(message, symbol_fname);
            strcat(message, "    ");
            strcat(message, symbol_name);*/

            StackTraceString += symbol_fname;
            StackTraceString += "\n";
            StackTraceString += symbol_name;
            StackTraceString += "\n";

        }
        free(demangled);
    }
    __android_log_print( ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", StackTraceString.c_str());
}

/*void SigActionHandler(int sig, siginfo_t* info, void* ucontext) {
.c_str()    const ucontext_t* signal_ucontext = (const ucontext_t*)ucontext;
    assert(signal_ucontext);

#if LIBUNWIND_WITH_REGISTERS_METHOD
    {
        printf("Backtrace captured using LIBUNWIND_WITH_REGISTERS_METHOD:\n");
        BacktraceState backtrace_state;
        BacktraceState_Init(&backtrace_state, signal_ucontext);
        LibunwindWithRegisters(&backtrace_state);
        PrintBacktrace(&backtrace_state);
    }
#endif

#if UNWIND_BACKTRACE_WITH_REGISTERS_METHOD
    {
        printf("Backtrace captured using UNWIND_BACKTRACE_WITH_REGISTERS_METHOD:\n");
        BacktraceState backtrace_state;
        BacktraceState_Init(&backtrace_state, signal_ucontext);
        UnwindBacktraceWithRegisters(&backtrace_state);
        PrintBacktrace(&backtrace_state);

    }
#endif

#if UNWIND_BACKTRACE_WITH_SKIPPING_METHOD
    {
        printf("Backtrace captured using UNWIND_BACKTRACE_WITH_SKIPPING_METHOD:\n");
        BacktraceState backtrace_state;
        BacktraceState_Init(&backtrace_state, signal_ucontext);
        UnwindBacktraceWithSkipping(&backtrace_state);
        PrintBacktrace(&backtrace_state);

    }
#endif

    exit(0);
}*/


void SetUpAltStack() {
    // Set up an alternate signal handler stack.

    __android_log_print(ANDROID_LOG_ERROR, "PVR-JNI-EH", "%s", "Setting Alternate Stack");
    stack_t stack = {};
    stack.ss_size = 0;
    stack.ss_flags = 0;
    stack.ss_size = SIGSTKSZ;
    stack.ss_sp = malloc(stack.ss_size);
    assert(stack.ss_sp);

    sigaltstack(&stack, NULL);
}

/*void SetUpSigActionHandler() {
    // Set up signal handler.
    struct sigaction action = {};
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = SigActionHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    sigaction(SIGSEGV, &action, NULL);
}*/