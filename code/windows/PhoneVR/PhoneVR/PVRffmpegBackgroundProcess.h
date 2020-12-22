
/*#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
*/
#include <string>
#include <iostream>
#include <windows.h> 
#include <stdio.h>
#include <string>

extern "C"
{
#include "x264.h"
}

#include <PVRGlobals.h>
#pragma warning( disable : 4800 ) // stupid warning about bool
#define BUFSIZE 256

class PVRffmpegBackgroundProcess
{
	int width = 1920, height = 1080;
	int64_t pts = 0;
	PROCESS_INFORMATION piProcInfo;

	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;
	HANDLE g_hChildStd_ERR_Rd = NULL;
	HANDLE g_hChildStd_ERR_Wr = NULL;
	HANDLE g_hChildStd_IN_Rd = NULL;
	HANDLE g_hChildStd_IN_Wr = NULL;

	bool bProcessStarted = false;

	PROCESS_INFORMATION CreateChildProcess(void);
public:
	PVRffmpegBackgroundProcess() {}
	~PVRffmpegBackgroundProcess() {}
	/*int initffmpeg();
	int Resample();
	int Encode(AVFrame* scaled_frame);
	int MuxAV(AVCodecContext* encoder);
	int Encode();
	int MuxAV();
	int startStream();*/
	
	int initFFMPEGChildProcess();
	void startSTDOutPipeReader();
	void startSTDErrPipeReader();
	int CreateFFMPEGChildProcess();

	bool isChildProcessRunning();
	std::string GetLastErrorAsString();
	
	void ReadFromSTDOutPipe(PROCESS_INFORMATION piProcInfo);
	void ReadFromSTDErrPipe(PROCESS_INFORMATION piProcInfo);
	int WriteToPipe(void * data, int datalen);
	float WriteToPipe(x264_picture_t * pic, int dataPerPlane);
};