#pragma once
#include "Eigen\Eigen"

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")

extern "C" {
#include "libavcodec\avcodec.h"
}

//extern AVFrame *pvrVFrame;

//void PVRInitStreamer(std::string ip, int devPort, int bitrate, int fps, float warp, int width, int height);
//void PVRCloseStreamer();
//void PVRSendFrame(Eigen::Quaternionf &quat);