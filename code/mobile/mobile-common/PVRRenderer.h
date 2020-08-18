#pragma once

#include "vr/gvr/capi/include/gvr.h"

#ifdef __cplusplus

namespace PVR {
    extern std::unique_ptr <gvr::GvrApi> gvrApi;
}

extern "C" {
#endif

extern float fpsRenderer;

unsigned int PVRInitSystem(int maxWidth, int maxHeight, float offFov, bool reproj, bool debug); // return render texture id
void PVRRender(int64_t pts);
void PVRTrigger();
void PVRPause();
void PVRResume();
void PVRDestroyGVR();

#ifdef __ANDROID__
void PVRCreateGVR(gvr_context *gvr_ctx);
#else
void PVRCreateGVRAndContext();
#endif

#ifdef __cplusplus
}
#endif

