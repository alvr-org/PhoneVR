#ifndef PHONEVR_COMMON_H
#define PHONEVR_COMMON_H

#include <jni.h>
extern JavaVM *jVM;

#define FUNC(type, func) extern "C" JNIEXPORT type JNICALL Java_viritualisres_phonevr_Wrap_##func

#endif   // PHONEVR_COMMON_H
