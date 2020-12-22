#include "PVRffmpegBackgroundProcess.h"
/*
int PVRffmpegBackgroundProcess::initffmpeg()
{
	return 0;
}

int PVRffmpegBackgroundProcess::Resample()
{
	// Source and Target WxH will be same in PVR
	struct SwsContext* scaler = sws_getContext(
		width, height, AV_PIX_FMT_YUYV422,
		width, height, AV_PIX_FMT_YUV422P,
		SWS_BICUBIC, NULL, NULL, NULL
	);

	AVFrame* scaled_frame = av_frame_alloc();

	scaled_frame->format = AV_PIX_FMT_YUV422P;
	scaled_frame->width = width;
	scaled_frame->height = height;

	av_image_alloc(	scaled_frame->data, scaled_frame->linesize,	scaled_frame->width, scaled_frame->height,
		(AVPixelFormat) scaled_frame->format, 16);

	/*sws_scale(scaler,
		(const uint8_t * const*)source_data, source_linesize,
		0, source_height,
		scaled_frame->data, scaled_frame->linesize);

	av_frame_free(&scaled_frame);
	return 0;
}

int PVRffmpegBackgroundProcess::Encode(AVFrame* scaled_frame)
{
	avformat_network_init();
	avcodec_register_all();

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	AVCodecContext* encoder = avcodec_alloc_context3(codec);

	encoder->bit_rate = 10 * 1000 * 10000;
	encoder->width = 1920;
	encoder->height = 1080;
	encoder->time_base = AVRational { 1, 60 };
	encoder->gop_size = 30;
	encoder->max_b_frames = 1;
	encoder->pix_fmt = AV_PIX_FMT_YUV422P;

//	av_opt_set(encoder->av_codec_context->priv_data, "preset", "ultrafast", 0);

	avcodec_open2(encoder, codec, NULL);

	AVFrame* raw_frame = scaled_frame;

	raw_frame->pts = pts++;
	avcodec_send_frame(encoder, raw_frame);

	av_freep(&raw_frame->data[0]);
	av_frame_free(&raw_frame);

	return 0;
}

int PVRffmpegBackgroundProcess::MuxAV(AVCodecContext* encoder)
{
	AVFormatContext* muxer = avformat_alloc_context();

	muxer->oformat = av_guess_format("matroska", "rtp://127.0.0.1:40000", NULL);

	AVStream* video_track = avformat_new_stream(muxer, NULL);
	AVStream* audio_track = avformat_new_stream(muxer, NULL);
	muxer->oformat->video_codec = AV_CODEC_ID_H264;
	muxer->oformat->audio_codec = AV_CODEC_ID_OPUS;

	avcodec_parameters_from_context(video_track->codecpar, encoder);
	video_track->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;

	video_track->time_base = AVRational { 1, 60 };
	video_track->avg_frame_rate = AVRational { 60, 1 };

	AVDictionary *options = NULL;
	av_dict_set(&options, "live", "1", 0);
	avformat_write_header(muxer, &options);
	return 0;
}

int PVRffmpegBackgroundProcess::startStream()
{
	return 0;
}
*/

int PVRffmpegBackgroundProcess::initFFMPEGChildProcess()
{
	//int main(int argc, char *argv[]) {
	SECURITY_ATTRIBUTES sa;
	printf("\n->Start of parent execution.\n");
	// Set the bInheritHandle flag so pipe handles are inherited. 
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDERR. 
	if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &sa, 0)) {
		exit(1);
	}
	// Ensure the read handle to the pipe for STDERR is not inherited.
	if (!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
		exit(1);
	}

	// Create a pipe for the child process's STDOUT. 
	if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)) {
		exit(1);
	}
	// Ensure the read handle to the pipe for STDOUT is not inherited
	if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
		exit(1);
	}

	if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 16358400)) {
		exit(1);
	}
	if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
		exit(1);
	}
	return 0;
}

void PVRffmpegBackgroundProcess::startSTDOutPipeReader()
{
	PVR_DB("PVRffmpegBackgroundProcess::startSTDOutPipeReader() Starting ...");
	std::thread* iCProcessReader = new std::thread([=] {
		ReadFromSTDOutPipe(piProcInfo);
	});
}

void PVRffmpegBackgroundProcess::startSTDErrPipeReader()
{
	PVR_DB("PVRffmpegBackgroundProcess::startSTDErrPipeReader() Starting ...");
	std::thread* iCProcessReader = new std::thread([=] {
		ReadFromSTDErrPipe(piProcInfo);
	});
}

int PVRffmpegBackgroundProcess::CreateFFMPEGChildProcess()
{
	if (bProcessStarted) 
		return 1;
	// Create the child process. 
	piProcInfo = CreateChildProcess();
	bProcessStarted = true;

	// Read from pipe that is the standard output for child process. 
	//printf("\n->Contents of child process STDOUT:\n\n", argv[1]);

	PVR_DB("PVRffmpegBackgroundProcess::CreateFFMPEGChildProcess() Created child Process");

	// The remaining open handles are cleaned up when this process terminates. 
	// To avoid resource leaks in a larger application, 
	//   close handles explicitly.
	return 0;
	//}
}

bool PVRffmpegBackgroundProcess::isChildProcessRunning()
{
	return bProcessStarted;
}


// Create a child process that uses the previously created pipes
//  for STDERR and STDOUT.
PROCESS_INFORMATION PVRffmpegBackgroundProcess::CreateChildProcess() 
{
	std::wstring szPath = L"C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe";
	std::wstring szCmdline = L"ffmpeg -re -f rawvideo -i pipe: -vcodec H264 -an -f rtp rtp://127.0.0.1:40000";

	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	bool bSuccess = FALSE;

	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDERR and STDOUT handles for redirection.
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_ERR_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bSuccess = CreateProcess(szPath.c_str(),
		L"ffmpeg -re -f rawvideo -video_size 1704x960 -r 60 -i pipe:0 -vcodec h264 -an -f rtp rtp://127.0.0.1:40000",     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo);  // receives PROCESS_INFORMATION
	CloseHandle(g_hChildStd_ERR_Wr);
	CloseHandle(g_hChildStd_OUT_Wr);
	CloseHandle(g_hChildStd_IN_Rd);
	// If an error occurs, exit the application. 
	if (!bSuccess) {
		PVR_DB("PVRffmpegBackgroundProcess::CreateChildProcess(): Could not create PVRffmpeg Child Process... Exiting... Error: " + GetLastErrorAsString());
		exit(1);
	}
	return piProcInfo;
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string PVRffmpegBackgroundProcess::GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return (std::to_string(errorMessageID) + ": " + message);
}

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data. 
void PVRffmpegBackgroundProcess::ReadFromSTDOutPipe(PROCESS_INFORMATION piProcInfo)
{
	DWORD dwRead;
	CHAR chBuf[BUFSIZE];
	bool bSuccess = FALSE;
	std::string out = "", err = "";
	while (1)
	{
		for (;;) 
		{
			bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
			if (!bSuccess || dwRead == 0) break;

			std::string s(chBuf, dwRead);

			PVR_DB("stdout: " + s);
			out += s;
		}
		//std::cout << "stdout:" << out << std::endl;
		//std::cout << "stderr:" << err << std::endl;
		//PVR_DB("stdout: " + out);
		//PVR_DB("stderr: " + err);
	}
}

void PVRffmpegBackgroundProcess::ReadFromSTDErrPipe(PROCESS_INFORMATION piProcInfo)
{
	DWORD dwRead;
	CHAR chBuf[64];
	bool bSuccess = FALSE;
	std::string out = "";//, err = "";
	while (1)
	{
		for (;;)
		{
			bSuccess = ReadFile(g_hChildStd_ERR_Rd, chBuf, 64, &dwRead, NULL);
			if (!bSuccess || dwRead == 0) break;

			std::string s(chBuf, dwRead);

			PVR_DB(s + " -- Child:stderr" );
			//err += s;

		}
		//std::cout << "stdout:" << out << std::endl;
		//std::cout << "stderr:" << err << std::endl;
		//PVR_DB("stdout: " + out);
		//PVR_DB("stderr: " + err);
	}
}

int PVRffmpegBackgroundProcess::WriteToPipe(void* data, int datalen)
{
	DWORD dwWritten;
	bool bSuccess = FALSE;
	bSuccess = WriteFile(g_hChildStd_IN_Wr, data, datalen, &dwWritten, NULL);

	return bSuccess ? dwWritten : -1;
}

float PVRffmpegBackgroundProcess::WriteToPipe(x264_picture_t* pic, int dataPerStride /*height, should be <= 65,535 bytes */)
{
	DWORD dwWritten;
	bool bSuccess = FALSE;

	int totalData = dataPerStride * (pic->img.i_stride[0] + pic->img.i_stride[1] + pic->img.i_stride[2]);
	int dataWritten = 0;
	for (int i = 0; i < pic->img.i_plane; i++)
	{		
		for (int j = 0; j < pic->img.i_stride[i]; j++)
		{
			bSuccess = WriteFile(g_hChildStd_IN_Wr, pic->img.plane[i] + (j*dataPerStride), dataPerStride, &dwWritten, NULL);
			if (bSuccess)
			{
				dataWritten += (int)dwWritten;
			}
			else
			{
				PVR_DB("PVRffmpegBackgroundProcess::WriteToPipe() Writing .. Plane : " + std::to_string(i)  + 
					", Stride: " + std::to_string(j) + ", DataperString: " +
					std::to_string(dataPerStride) + ", Retrun: " +
					std::to_string(bSuccess) + ", Written: " +
					std::to_string(dwWritten) + ", Last Error: " + GetLastErrorAsString());
			}
		}
	}

	return ((float)dataWritten/totalData);
}
