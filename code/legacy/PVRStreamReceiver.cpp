#include "PVRStreamReceiver.h"

#include <stdlib.h>
#include "PVRGlobals.h"
#include "PVRMobileGlobals.h"

extern "C" {
//#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

using namespace std;

namespace {
    AVCodecContext *vc;
    AVFrameSideData *sd;
    AVFrame *vFrame = nullptr;
    int gotFrame = 0;
    AVPacket *pkt;

void PVRInitReceiveStreams(uint16_t port, bool(*getBuffer)(uint8_t **buf), void(*releaseFilledBuf)(size_t sampSz, bool eos), void(*unwindSegue)()) {
    av_register_all();
    avformat_network_init();

    AVDictionary *d = nullptr;
    av_dict_set(&d, "rtsp_flags", "listen", 0);

    AVFormatContext *oc = avformat_alloc_context();
    enableSendPose = 1;
    int res = avformat_open_input(&oc, ((string)"rtsp://" + pvrDevIP + ":" + to_string(port)).c_str(), nullptr, &d);
    if (res != 0) {
        exit(1);
    }

    if (avformat_find_stream_info(oc, nullptr) < 0) {
        exit(1);
    }

    int videoIdx = -1, audioIdx = -1;
    for (int i = 0; i < oc->nb_streams; i++) {
        if (oc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = i;
        }
        else if (oc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIdx = i;
        }
    }

    //video initialization

    vFrame = av_frame_alloc();
    vc = avcodec_alloc_context3(nullptr);
    avcodec_parameters_to_context(vc, oc->streams[videoIdx]->codecpar);
    AVCodec *vCodec = avcodec_find_decoder(vc->codec_id);
    if (vCodec != nullptr && avcodec_open2(vc, vCodec, nullptr) == 0) {
        pvrFrameWidth[0] = vc->width;
        pvrFrameHeight[0] = vc->height;
        pvrFrameWidth[1] = pvrFrameWidth[2] = vc->width / 2;
        pvrFrameHeight[1] = pvrFrameHeight[2] = vc->height / 2;
        pvrVSizeReady = 1;

        pkt = av_packet_alloc();
        //receive loop
        while (pvrRunning && av_read_frame(oc, pkt) == 0) {
            if (pkt->stream_index == videoIdx) {

                uint8_t *buf = nullptr;
                if (getBuffer(&buf)) {
                    //auto sz = pkt->size;
                    auto sz = pkt->buf->size;
                    copy(pkt->buf->data, pkt->buf->data + sz, buf);
                    releaseFilledBuf((size_t)sz, false);
                }

                /*
                //pkt->side_data_elems
                pvrVMutex.lock();
                //int err = avcodec_send_packet(vc, &pkt);
                avcodec_decode_video2(vc, vFrame, &gotFrame, pkt);
                if (gotFrame) {
                    for (int i = 0; i < 3; ++i)
                        pvrVFrame[i] = vFrame->data[i];
                }
                pvrVMutex.unlock();*/
            }
        }
        // send EOS message
        //getBuffer();
        //releaseFilledBuf(0, true);
    }

    pvrVMutex.lock();
    pvrRunning = 0; // if network error or end of stream
    avcodec_close(vc);
    //avcodec_close(ac);
    av_frame_free(&vFrame);
    av_packet_free(&pkt);
    avformat_close_input(&oc);
    pvrVMutex.unlock();

    unwindSegue();
}

//mutex handled by caller
bool pvrGetVFrame() {
    if (gotFrame && vFrame != nullptr) {
        //pvrVMutex.lock();
        //bool received = avcodec_receive_frame(vc, vFrame) == 0;
        //for (int i = 0; i < 3; ++i)
         //   pvrVFrame[i] = vFrame->data[i];

        sd = av_frame_get_side_data(vFrame, AV_FRAME_DATA_A53_CC);
        if (sd != nullptr)// side data have been populated
            memcpy(recOrientQuat, sd->data, 4 * 4);
        //pvrVMutex.unlock();
        gotFrame = 0;
        return true;
    }
    return false;
}

}