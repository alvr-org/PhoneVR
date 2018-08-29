
#include "PVRStreamer.h"
#include "PVRFileManager.h"
#include "PVRDXManager.h"
#include "PVRMath.h"

#include <string>

extern "C" {
#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libavutil\imgutils.h"
#include "libavutil\opt.h"
}

using namespace std;

AVFrame *pvrVFrame;

namespace {

    AVFormatContext *oc;
    AVStream *vStream;
    AVCodec *vCodec;
    AVCodecContext *vCtx;
    int FPS;
    const AVPixelFormat VFMT = AV_PIX_FMT_YUV420P;
    const AVCodecID VCODEC_ID = AV_CODEC_ID_H264;


    AVFrameSideData *sd;
    AVPacket *pkt;// need to stay outside, or else encode error

    //mutex vmutex;
 /*   bool enabled = false;
    bool rendered = false;*/

    //float vwarp;
}

void createVStream(int64_t bitrate, int width, int height) {
    vCodec = avcodec_find_encoder_by_name("libx264");
    vStream = avformat_new_stream(oc, vCodec);
    vStream->id = oc->nb_streams - 1;
    vStream->time_base = { 1, FPS };
    vCtx = vStream->codec;

    vCtx->bit_rate = bitrate;
    vCtx->width = width;
    vCtx->height = height;
    vCtx->time_base = { 1, FPS };
    vCtx->framerate = { FPS, 1 };
    vCtx->gop_size = FPS / 2;
    vCtx->pix_fmt = VFMT;
    vCtx->thread_count = 8;


    int err = av_opt_set(vCtx->priv_data, "preset", "ultrafast", 0);
    err = av_opt_set(vCtx->priv_data, "tune", "zerolatency", 0);

    vCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;


    avcodec_open2(vCtx, vCodec, nullptr);
    pvrVFrame = av_frame_alloc();
    pvrVFrame->format = VFMT;
    pvrVFrame->width = width;
    pvrVFrame->height = height;
    av_frame_get_buffer(pvrVFrame, 32);
    sd = av_frame_new_side_data(pvrVFrame, AV_FRAME_DATA_A53_CC, 20); // using the subtitles slot to return orientation data

    pkt = av_packet_alloc();
}

long counter = 0;

void PVRSendFrame(Eigen::Quaternionf &quat) {
    float lastOrient[4] = { quat.w(), quat.x(), quat.y(), quat.z() };
    memcpy(sd->data, lastOrient, 4 * 4);

    int gotOutp, ret = 0;

    av_init_packet(pkt);
    pvrVFrame->pts = counter;
    counter++;

    ret = avcodec_encode_video2(vCtx, pkt, pvrVFrame, &gotOutp);

    if (gotOutp/* && !err*/) {
        av_write_frame(oc, pkt);
        //err = avcodec_encode_video2(vCtx, &pkt, vFrame, &gotOutp);
        av_packet_unref(pkt);
    }
    //av_packet_unref(&pkt);
}

void PVRInitStreamer(string ip, int devPort, int bitrate, int fps, float warp, int width, int height) {
    //av_log_set_level(AV_LOG_DEBUG);
    FPS = fps;
    //vwarp = warp;

    auto uri = ((string)"rtsp://" + ip + ":" + to_string(devPort));//.c_str();
    PVR_DB("url: "s + uri);

    av_register_all();
    avformat_network_init();
    avformat_alloc_output_context2(&oc, nullptr, "rtsp", uri.c_str());////////// todo: impose output format
    auto fmt = oc->oformat;


    createVStream(bitrate, width, height);

    //av_dump_format(oc, 0, devIP, 1);

    int ret = avformat_write_header(oc, nullptr);
    if (ret < 0) {
        PVR_DB("error writing header: "s + to_string(ret));
    }
}

void PVRCloseStreamer() {
    /*  if (enabled) {*/
          //enabled = false;
          //rendered = true; // unblock thread
          //vthr->join();
          //delete vthr;

    av_write_trailer(oc);

    avcodec_close(vCtx);
    //avcodec_free_context(&vCtx);
    //av_free(vFrame->data[0]);
    av_frame_free(&pvrVFrame);
    av_packet_free(&pkt);

    avio_close(oc->pb);

    avformat_free_context(oc);
    //}
    PVR_DB("Video stream destroyed");
}
