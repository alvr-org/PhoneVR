#pragma once
#include <iostream>
#include <d3d11.h>

extern "C"
{
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavutil/hwcontext_d3d11va.h"
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avdevice.lib")

#include "PVRMath.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "openvr_driver.h"

#define RELEASE(obj) if(obj) { obj->Release(); obj = nullptr; }
#define OK_OR_EXEC(call, func) if(FAILED(call)) func()

using namespace std;
using namespace Eigen;
using namespace vr;

class PVRffmpeg
{
	int width, height, remotePort;
	string remoteIP;

	mutex mgfxSharedHandle;
	SharedTextureHandle_t gfxSharedHandle = INVALID_SHARED_TEXTURE_HANDLE;
	Quaternionf quat;

	AVFormatContext *OutputContext;
	AVCodec *vVideoCodech264;
	AVCodec *vVideoCodecNVENC; // This is Encoder
	AVStream *OutputVideoStream;
	AVCodecContext *OutputEncoderContext;

	AVBufferRef *nvencD3D11DeviceContext = NULL;
	AVBufferRef *nvencHWFramesContext = NULL;

	AVHWDeviceContext *nvencD3D11DeviceCtx = NULL;
	AVD3D11VADeviceContext* AVD3DhwDevice = NULL;

	ID3D11Device        *avHWDevice = NULL;
	ID3D11DeviceContext *avHWDevice_context = NULL;

	ID3D11Texture2D* stagingTex = NULL;

	void encode(AVCodecContext * enc_ctx, AVFrame * frame, AVPacket * pkt, AVFormatContext * outputContext);

public:
	PVRffmpeg();
	~PVRffmpeg();

	void setSettings(int width, int height, string remoteIP, int port);
	void Stream();
	void StopStreaming();
	void ProcessSharedHandle(SharedTextureHandle_t gfxSharedHandle, Quaternionf quat);
	void debugHr(HRESULT hr);
};
