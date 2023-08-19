#include "PVRSocketUtils.h"
#include <iostream>

using namespace std;
using namespace std::this_thread;
using namespace asio;
using namespace asio::ip;
using namespace std::chrono;

// packetize TCP stream: "pvr" / msg type byte / data size (2 bytes) / data
//  prefix -> 6 bytes

TCPTalker::TCPTalker(uint16_t port,
                     function<void(PVR_MSG, vector<uint8_t>)> recCb,
                     function<void(std::error_code)> errCb,
                     bool isServer,
                     string ip) {
    bool initted = false;
    PVR_DB_I("[TCPTalker::TCPTalker] Init'ed with p:" + to_string(port) +
             " isServer:" + to_string(isServer) + " ip:" + to_string(ip))

    thr = new std::thread([=, &initted] {
        try {
            io_service svc;
            tcp::socket skt(svc);
            _skt = &skt;
            tcp::acceptor acc(svc, {tcp::v4(), port});

            asio::error_code ec = asio::error::fault;
            auto errHdl = [&](const asio::error_code &err) { ec = err; };
            if (isServer) {   // talker is a server; Mobile device will be Announcing UDP + Will be
                              // Server
                if (ip.empty()) {
                    acc.async_accept(skt, errHdl);   // todo: simplify
                    PVR_DB_I("[TCPTalker::TCPTalker] Talker is Server and ip is Empty. Accepting "
                             "All connection...");
                } else {
                    acc.async_accept(skt, errHdl);   // todo: simplify
                    // skt.async_connect({ address::from_string(ip), port }, errHdl);
                    PVR_DB_I("[TCPTalker::TCPTalker] Talker is Server and Accepting connections "
                             "from ip:" +
                             ip + " & port:" + to_string(port) + "...");
                }
                initted = true;   // early unblock server
            } else   // talker is a client; Desktop will be Client will receive the Announcement +
                     // Will try to connect.
            {
                skt.async_connect({address::from_string(ip), port}, errHdl);
                PVR_DB_I("[TCPTalker::TCPTalker] Talker is Client and Connecting to ip:" + ip +
                         " & port:" + to_string(port) + "...");
            }
            svc.run();
            svc.reset();
            if (!isServer)
                initted = true;   // on server mode this goes out of scope

            if (ec.value() == 0) {
                IP = skt.remote_endpoint().address().to_string();

                const size_t bufsz = 256;
                string prefix = "pvr";
                uint8_t buf[bufsz];
                vector<uint8_t> stream;
                size_t msgLen = 0;

                function<void(const asio::error_code &, size_t)> handle = [&](const asio::error_code
                                                                                  & /*err*/,
                                                                              size_t len) {
                    // PVR_DB_I("[TCPTalker::TCPTalker] Revd some data... Interpreting...");
                    stream.insert(stream.end(), buf, buf + len);
                    bool loop = true;
                    while (loop) {
                        auto it = find_first_of(
                            stream.begin(), stream.end(), prefix.begin(), prefix.end());
                        if (stream.end() - it >= 6) {
                            msgLen = size_t(*(it + 4)) + size_t(*(it + 5)) * 0x100;
                            it += 6;   // skip prefix
                            if (stream.size() >=
                                it - stream.begin() +
                                    msgLen) {   // warning! do not increment iterator out of bounds
                                recCb(PVR_MSG(*(it - 3)), vector<uint8_t>(it, it + msgLen));
                                stream.erase(stream.begin(), it + msgLen);
                            } else
                                loop = false;
                        } else
                            loop = false;
                    }
                    skt.async_read_some(buffer(buf, bufsz), handle);
                };
                skt.async_read_some(buffer(buf, bufsz), handle);
                PVR_DB_I("[TCPTalker::TCPTalker] Talker is Connected. Trying to read some data...");
                svc.run();
            }
            if (ec.value() != 0)
                errCb(ec);

            sktMtx.lock();
            skt.close();
            _skt = nullptr;
            sktMtx.unlock();
        } catch (exception &e) {
            PVR_DB_I("[TCPTalker::TCPTalker] Exception caught: " + string(e.what()));
        }
    });
    while (!initted)
        sleep_for(milliseconds(2));
}

void TCPTalker::safeDispatch(function<void()> hdl) {
    try {
        sktMtx.lock();
        if (_skt) {
            bool done = false;
            _skt->get_io_service().dispatch([&] {
                hdl();
                done = true;
            });
            while (!done)
                sleep_for(milliseconds(10));
        }
        sktMtx.unlock();
    } catch (exception &e) {
        PVR_DB_I("[TCPTalker::safeDispatch] Exception caught: " + string(e.what()));
    }
}

bool TCPTalker::send(PVR_MSG msgType, vector<uint8_t> data) {
    vector<uint8_t> buf = {'p', 'v', 'r', (uint8_t) msgType, 0, 0};
    auto sz = data.size();
    memcpy(&buf[4], &sz, 2);
    buf.insert(buf.end(), data.begin(), data.end());

    bool success = false;
    safeDispatch([&] {
        if (_skt->is_open()) {
            asio::error_code ec;
            write(*_skt, buffer(buf), ec);
            success = ec.value() == 0;
            if (ec.value()) {
                PVR_DB_I("[TCPTalker::send] Error Sending EC(" + to_string(ec.value()) +
                         "): " + ec.message());
            } else {
                PVR_DB_I("[TCPTalker::send] Msg Sent with ID:" + to_string(buf[3]));
            }
        } else {
            PVR_DB_I("[TCPTalker::send] Error Sending: Socket is not open");
        }
    });
    return success;
}

TCPTalker::~TCPTalker() {
    safeDispatch([=] { _skt->get_io_service().stop(); });
    EndThread(thr);
}

typedef unsigned long uint32;

uint32 SockAddrToUint32(struct sockaddr *a) {
    return ((a) && (a->sa_family == AF_INET)) ? ntohl(((struct sockaddr_in *) a)->sin_addr.s_addr)
                                              : 0;
}

// convert a numeric IP address into its string representation
void Inet_NtoA(uint32 addr, char *ipbuf) {
    sprintf(ipbuf,
            "%li.%li.%li.%li",
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            (addr >> 0) & 0xFF);
}