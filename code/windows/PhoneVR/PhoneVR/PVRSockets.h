#pragma once

#include "Geometry"
#include "openvr_driver.h"

#include "PVRGlobals.h"
#include "PVRSocketUtils.h"

void PVRStartConnectionListener(std::function<void(std::string ip, PVR_MSG devType)> callback);
void PVRStopConnectionListener();

void PVRStartStreamer(std::string ip, uint16_t width, uint16_t height, std::function<void(std::vector<uint8_t>)> headerCb, std::function<void()> onErrCb);
void PVRProcessFrame(uint64_t hdl, Eigen::Quaternionf quat);
void PVRStopStreamer();

void PVRStartReceiveData(std::string ip, vr::DriverPose_t *pose, uint32_t *objId);
void PVRStopReceiveData();
