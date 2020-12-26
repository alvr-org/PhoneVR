#include "PVRffmpeg.h"

void ffmpeg_log_callback(void* classptr, int LogLevel, const char* printfString, va_list args)
{
	char buffer[256];
	buffer[0] = '\0';

	vsprintf(buffer, printfString, args);
	PVR_DB("FFMPEG LOG: " + string(buffer));
}

PVRffmpeg::PVRffmpeg()
{
	av_log_set_flags(AV_LOG_INFO); 
	av_log_set_callback(&ffmpeg_log_callback);

	/* Calling SetDllDirectory with the empty string (but not NULL) removes the
	 * current working directory from the DLL search path as a security pre-caution. */
	//SetDllDirectory(L"");

	avdevice_register_all();
	avformat_network_init();
}

PVRffmpeg::~PVRffmpeg()
{
	avformat_network_deinit();
}

void PVRffmpeg::setSettings(int width, int height, string remoteIP, int port)
{
	this->width = width;
	this->height = height;
	this->remoteIP = remoteIP;
	this->remotePort = port;	
	//AVCodec *avc = avcodec_find_encoder_by_name("H264");
}

extern "C"
{
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
}

void PVRffmpeg::ProcessSharedHandle(SharedTextureHandle_t igfxSharedHandle, Quaternionf iquat)
{
	mgfxSharedHandle.lock();

	gfxSharedHandle = igfxSharedHandle;
	quat = iquat;
	
	if (OutputEncoderContext)
	{
		AVPacket *pkt = av_packet_alloc();
		if (!pkt)
		{
			PVR_DB("PVRffmpeg::ProcessSharedHandle() Could not allocate pkt");
			//exit(1);
		}

		AVFrame* frame = av_frame_alloc();
		if (!frame) {
			PVR_DB("PVRffmpeg::ProcessSharedHandle() Could not allocate video frame");
			//exit(1);
		}
		frame->format = OutputEncoderContext->pix_fmt;
		frame->width = OutputEncoderContext->width;
		frame->height = OutputEncoderContext->height;
		
		//int ret = av_frame_get_buffer(frame, 0);
		/*int align = 0;

		const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
		int ret, i, padded_height, total_size;
		int plane_padding = 32;
		ptrdiff_t linesizes[4];
		size_t sizes[4];

		if (!desc)
		{
			PVR_DB("av_pix_fmt_desc_get failed : " + to_string(  AVERROR(EINVAL) ) );
		}

		if ((ret = av_image_check_size(frame->width, frame->height, 0, NULL)) < 0)
		{
			PVR_DB("av_image_check_size failed : " + to_string(ret));
		}

		if (!frame->linesize[0]) {
			if (align <= 0)
				align = 32;

			for (i = 1; i <= align; i += i) {
				ret = av_image_fill_linesizes(frame->linesize, (AVPixelFormat)frame->format,
					FFALIGN(frame->width, i));
				if (ret < 0)
				{
					PVR_DB("av_image_fill_linesizes failed : " + to_string(ret));
				}
				if (!(frame->linesize[0] & (align - 1)))
					break;
			}

			for (i = 0; i < 4 && frame->linesize[i]; i++)
				frame->linesize[i] = FFALIGN(frame->linesize[i], align);
		}

		for (i = 0; i < 4; i++)
			linesizes[i] = frame->linesize[i];

		padded_height = FFALIGN(frame->height, 32);
		if ((ret = av_image_fill_plane_sizes(sizes, (AVPixelFormat)frame->format,
			padded_height, linesizes)) < 0)
		{
			PVR_DB("av_image_fill_plane_sizes failed : " + to_string(ret));
		}

		total_size = 4 * plane_padding;
		for (i = 0; i < 4; i++) {
			if (sizes[i] > INT_MAX - total_size)
			{
				PVR_DB("total_size plane_padding failed : " + to_string(AVERROR(EINVAL)));
			}
			total_size += sizes[i];
		}

		frame->buf[0] = av_buffer_alloc(total_size);
		if (!frame->buf[0]) {
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		if ((ret = av_image_fill_pointers(frame->data, (AVPixelFormat)frame->format, padded_height,
			frame->buf[0]->data, frame->linesize)) < 0)
			goto fail;

		for (i = 1; i < 4; i++) {
			if (frame->data[i])
				frame->data[i] += i * plane_padding;
		}

		frame->extended_data = frame->data;	

		if (ret < 0) {
			char errbuf[124];

			av_strerror(AVERROR(ret), errbuf, 124);
			PVR_DB("PVRffmpeg::Stream() Could not allocate the frame data Error (" + to_string(ret) + "): " + errbuf );
			//exit(1);
		}*/
		int ret = av_frame_make_writable(frame);
		if (ret < 0)
		{
			PVR_DB("PVRffmpeg::ProcessSharedHandle() Could not make AV_FRame Writable");
			//exit(1);
		}

		//ID3D11Texture2D* hwTexture = (ID3D11Texture2D *)(frame->data[0]);
		//DXGI_FORMAT_NV12
		//**** Loop This for fps Times
		
		ID3D11Texture2D *inpTex;
		HRESULT hr = avHWDevice->OpenSharedResource((HANDLE)gfxSharedHandle, __uuidof(ID3D11Texture2D), (void **)&inpTex);
		if (FAILED(hr))
		{
			PVR_DB("PVRffmpeg::ProcessSharedHandle() Failed to OpenSharedResource, D3DHWDevFeatureLevel: " + to_string(avHWDevice->GetFeatureLevel()) );
			debugHr(hr);
			//return;
		}

		IDXGIKeyedMutex *dxMtx;
		hr = inpTex->QueryInterface(__uuidof(dxMtx), reinterpret_cast<void**>(&dxMtx));
		if (FAILED(hr))
		{
			PVR_DB("PVRffmpeg::ProcessSharedHandle() Failed to QueryInterface");
			debugHr(hr);
			//return;
		}

		if (SUCCEEDED(dxMtx->AcquireSync(0, 500))) {
			avHWDevice_context->CopyResource(stagingTex, inpTex); //texAmp is automatically updated
			dxMtx->ReleaseSync(0);
		}

		//ID3D11Texture2D* hwTexture = (ID3D11Texture2D *)(frame->data[0]);
		frame->data[0] = (uint8_t *)stagingTex;
		frame->data[1] = 0;

		RELEASE(dxMtx);
		RELEASE(inpTex);

		//****

		//(*hwTexture).GetDevice(&hwDevice);

		/*ID3D11DeviceContext* hwDeviceCtx;
		hwDevice->GetImmediateContext(&hwDeviceCtx);

		ID3D11Texture2D* sharedTexture;

		HRESULT hr = hwDevice->OpenSharedResource((HANDLE) gfxSharedHandle, __uuidof(ID3D11Texture2D), (void**)&inpTex);
		if (FAILED(hr))
		{
			PVR_DB("PVRffmpeg::Stream() Failed to obtaion open shared resource.");
			return;
		}

		int index = *frame->data[1];
		hwDeviceCtx->CopySubresourceRegion(sharedTexture, 0, 0, 0, 0, hwTexture, index, NULL);*/

		/* flush the encoder */
		encode(OutputEncoderContext, frame, pkt, OutputContext);

		/* add sequence end code to have a real MPEG file */
		//fwrite(endcode, 1, sizeof(endcode), f);
		//fclose(f);

		//avcodec_free_context(&OutputEncoderContext);

	fail:
		av_frame_unref(frame);
		{
			PVR_DB("failed : " + to_string(ret));
		}
		av_frame_free(&frame);
		av_packet_free(&pkt);
	}

	mgfxSharedHandle.unlock();
}

void  PVRffmpeg::debugHr(HRESULT hr) 
{
	LPWSTR output;
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&output, 0, NULL);
	PVR_DB_I(wstring(output));
}

void PVRffmpeg::Stream()
{
	//	av_dict_set(&sws_dict, "flags", "bicubic", 0);
	int result = avformat_alloc_output_context2(&OutputContext, NULL, "rtp", "rtp://127.0.0.1");

	// Using SW libx264 Codec
	//vVideoCodech264 = avcodec_find_encoder(AV_CODEC_ID_H264);
	vVideoCodecNVENC = avcodec_find_encoder_by_name("h264_nvenc"); // Type 2 Encoder	
	OutputVideoStream = avformat_new_stream(OutputContext, vVideoCodecNVENC);

	OutputEncoderContext = avcodec_alloc_context3(vVideoCodecNVENC);

	result = av_hwdevice_ctx_create(&nvencD3D11DeviceContext, AV_HWDEVICE_TYPE_D3D11VA, "d3d11va", NULL, 0);
	nvencD3D11DeviceCtx = (AVHWDeviceContext*)nvencD3D11DeviceContext->data;

	nvencHWFramesContext = av_hwframe_ctx_alloc(nvencD3D11DeviceContext);
	
	AVHWFramesContext* AVHWFramesCTx = (AVHWFramesContext *)nvencHWFramesContext->data;
	AVHWFramesCTx->format = AV_PIX_FMT_D3D11;

	OutputEncoderContext->hw_frames_ctx = nvencHWFramesContext;

	if (!OutputEncoderContext) 
	{
		PVR_DB("PVRffmpeg::Stream() Could not allocate video codec context");
		exit(1);
	}
	
	OutputEncoderContext->bit_rate = 400000;
	OutputEncoderContext->width = width;
	OutputEncoderContext->height = height;

	OutputEncoderContext->time_base = AVRational { 1, 60 };
	OutputEncoderContext->framerate = AVRational { 60, 1 };

	OutputEncoderContext->gop_size = 10;
	OutputEncoderContext->max_b_frames = 1;
	OutputEncoderContext->pix_fmt = AV_PIX_FMT_D3D11;

	PVR_DB("PVRffmpeg::Stream(): AVHWFramesPixFormate : " + to_string(AVHWFramesCTx->format) + " & OutputEncoderContext PixFmt: " + to_string(AV_PIX_FMT_D3D11));

	if (vVideoCodecNVENC->id == AV_CODEC_ID_H264)
		av_opt_set(OutputEncoderContext->priv_data, "preset", "slow", 0);

	/* open it */

	int ret = avcodec_open2(OutputEncoderContext, vVideoCodecNVENC, NULL);
	if (ret < 0) {
		PVR_DB("PVRffmpeg::Stream() Could not open codec: %s"+ to_string(ret));
		//exit(1);
	}
	
	AVD3DhwDevice = (AVD3D11VADeviceContext *)nvencD3D11DeviceCtx->hwctx;

	avHWDevice = AVD3DhwDevice->device;
	avHWDevice_context = AVD3DhwDevice->device_context;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	HRESULT hr = avHWDevice->CreateTexture2D(&texDesc, nullptr, &stagingTex);
	if (FAILED(hr))
	{
		PVR_DB_I("######### DirectX Error: #########");
		debugHr(hr);
	}
}

void PVRffmpeg::encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, AVFormatContext *outputContext)
{
	int ret;
	/* send the frame to the encoder */
	if (frame)
	{
		PVR_DB("PVRffmpeg::encode() Send frame %d" + to_string(frame->pts));
	}

	ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0) {
		PVR_DB("PVRffmpeg::encode() Error sending a frame for encoding");
		//exit(1);
	}
	while (ret >= 0) {
		ret = avcodec_receive_packet(enc_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			PVR_DB("PVRffmpeg::encode() Error during encoding");
			//exit(1);
		}
		PVR_DB("PVRffmpeg::encode() Write packet "+to_string(pkt->pts)+" (size=" + to_string(pkt->size)+ ")\n" );

		av_write_frame(outputContext, pkt);

		//fwrite(pkt->data, 1, pkt->size, outfile);
		av_packet_unref(pkt);
	}
}

void PVRffmpeg::StopStreaming()
{

}
