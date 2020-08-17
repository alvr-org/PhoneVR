#pragma once

#include "PVRGlobals.h"

#ifdef __cplusplus

#include <vector>
std::vector<float> DequeueQuatAtPts(int64_t pts);
void SendAdditionalData(std::vector<uint16_t> maxSize, std::vector<float> fov, float ipd);


struct EmptyVidBuf
{
    uint8_t *buf;
    int idx;
    size_t bufSz;
};

struct FilledVidBuf
{
    int idx;
    size_t pktSz;
    uint64_t pts;
};

extern "C" {
#endif

bool PVRIsVidBufNeeded();
void PVREnqueueVideoBuf(EmptyVidBuf emptyVidBuf);
FilledVidBuf PVRPopVideoBuf();

void PVRStartAnnouncer(const char *ip, uint16_t port, void(*segueCb)(), void(*headerCb)(uint8_t *, size_t), void(*unwindSegueCb)());
void PVRStopAnnouncer();

void PVRStartReceiveStreams(uint16_t port);
void PVRStopStreams();

void PVRStartSendSensorData(uint16_t port, bool (*getSensorData)(float *orQuat, float *acc));


// TODO: Make a .h file of Native-lib.cpp and add the below
void updateJavaTextViewFPS(float f1, float f2, float f3, float cf1, float cf2, float cf3);
#ifdef __cplusplus
}
#endif
