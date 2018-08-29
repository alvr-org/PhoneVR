#pragma once
#include "PVRGlobals.h"

#ifdef _WIN32
#define ASIO_MSVC _MSC_VER // workaround for asio bug on windows
#endif
#define ASIO_STANDALONE
#include "asio.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <map>
#include <type_traits>
#include <vector>
#include "Eigen/Geometry"

namespace PVR {

    class UDPTalker {
        std::string ip;
        u_short port;
        PVR_ERR error = PVR_ERR_NONE;
        std::vector<uint8_t> lastInData, lastOutData;
        int bufSz;

        // asio::ip::udp::endpoint sendEP;
        TimeBomb bomb;

    public:

        //if ip is null, outgoing packets are broadcasted. When a response arrives, its ip is used instead.
        UDPTalker(int port, std::string ip = "", int maxBufSize = 256) : port(port), lastOutData(maxBufSize), bufSz(maxBufSize), bomb([&] {
            error = PVR_ERR_UDP_TIMEOUT;
        }) { }

        ~UDPTalker() { error = PVR_ERR_UDP_DESTROYED; }

        const std::string getIP() { return ip; }

        void receive(std::function<void(std::vector<uint8_t> inData, PVR_ERR error)> callback, std::chrono::nanoseconds timeout = std::chrono::seconds(1), bool waitToFinish = false);

        // if the callback is not null, a receive thread starts.
        void send(std::vector<uint8_t> outData, std::chrono::nanoseconds sendInterval, std::function<void(std::vector<uint8_t> inData, PVR_ERR error)> callback = nullptr, std::chrono::nanoseconds timeout = std::chrono::seconds(1), bool waitToFinish = false);

        //stops any send or receive thread
        void stop() { error = PVR_ERR_UDP_MANUAL_STOP; } // call _stop syncronously, it will not take much time
    };
}
