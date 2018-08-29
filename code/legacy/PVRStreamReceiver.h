#pragma once

#include "PVRGlobals.h"
#include "PVRMobileGlobals.h"

#ifdef __cplusplus
extern "C" {

bool pvrGetVFrame();
#endif

//void PVRInitReceiveStreams(uint16_t port, bool(*getBuffer)(uint8_t **buf), void(*releaseFilledBuf)(size_t sampSz, bool eos), void(*unwindSegue)()) ;

#ifdef __cplusplus
}
#endif
