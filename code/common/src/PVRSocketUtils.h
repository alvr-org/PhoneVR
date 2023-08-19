#pragma once

#include <mutex>
#include <thread>

#include "PVRGlobals.h"

#define WIN32_LEAN_AND_MEAN
#define ASIO_STANDALONE
#include "asio.hpp"

enum PVR_MSG {
    PAIR_HMD,
    PAIR_PHONE_CTRL,
    PAIR_HMD_CTRLS,
    PAIR_ACCEPT,
    ADDITIONAL_DATA,
    HEADER_NALS,
    DISCONNECT,
};

class TCPTalker {
    std::thread *thr;
    std::mutex sktMtx;
    asio::ip::tcp::socket *_skt =
        nullptr;   // warning: any call to this must be wrapped in svc->post()
    std::string IP;

    void safeDispatch(std::function<void()> hdl);

  public:
    TCPTalker(uint16_t port,
              std::function<void(PVR_MSG msgType, std::vector<uint8_t> inData)> receiveCallback,
              std::function<void(std::error_code err)> errCb,
              bool isServer,
              std::string ip = "");
    ~TCPTalker();

    bool send(PVR_MSG msgType, std::vector<uint8_t> outData = {});

    std::string getIP() { return IP; }
};

typedef unsigned long uint32;

uint32 SockAddrToUint32(struct sockaddr *a);
void Inet_NtoA(uint32 addr, char *ipbuf);