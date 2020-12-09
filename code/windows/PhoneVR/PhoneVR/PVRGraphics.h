#pragma once
#include <vector>

void PVRInitDX();
void PVRUpdTexHdl(uint64_t texHdl, int whichBuffer);
void PVRStartGraphics(std::vector<std::vector<uint8_t *>> vvbuf, uint32_t width, uint32_t height);
void PVRStopGraphics();
void PVRReleaseDX();

extern float fpsRenderer;