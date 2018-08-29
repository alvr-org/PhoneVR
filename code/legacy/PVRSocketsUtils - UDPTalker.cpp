#include "PVRSocketsUtils.h"

using namespace std;
using namespace std::placeholders;
using namespace std::this_thread;
using namespace std::chrono;

using namespace asio;
using namespace ip;

typedef std::thread thr;

namespace PVR {

#define debugVec(v) PVR_DB(string(v.begin(), v.end()).c_str());

    void UDPTalker::receive(function<void(vector<uint8_t> inData, PVR_ERR error)> callback, nanoseconds timeout, bool waitToFinish) {
        error = PVR_ERR_NONE;

        if (timeout > seconds(0))
            bomb.reset(timeout, true);

        auto lambda = [=] {
            io_service svc;
            udp::socket skt(svc, {udp::v4(), (ushort)port});
            vector<uint8_t> buf(bufSz);
            udp::endpoint remSendEP;
            steady_timer st(svc);
            size_t pktSize = 0;
            auto oldbuf = buf;
            do {
                bool unblock = false;
                skt.async_receive_from(buffer(buf), remSendEP, bind([&](const asio::error_code& err, size_t pkt_sz) {
                    if (!unblock)
                        pktSize = pkt_sz;
                    st.cancel();
                }, _1, _2));
                st.expires_from_now(milliseconds(5));
                st.async_wait(std::bind([&] {
                    unblock = true;
                    skt.cancel();
                }));
                svc.run();
                svc.reset();
            }
            while ((pktSize <= 0 || buf == lastInData || buf == lastOutData) && error == PVR_ERR_NONE); // force continue if dummy packet, received again same packet or self sent packet
            bomb.abort_async();

            lastInData = buf; // copy
            if (error != PVR_ERR_NONE)
                lastInData.clear();
            else
                ip = remSendEP.address().to_string();

            callback(vector<uint8_t>(&buf[0], &buf[0] + pktSize), error); // execute callback
            error = PVR_ERR_UNKNOWN; // unblock sender
        };

        if (!waitToFinish)
            thr(lambda).detach();
        else
            lambda();
    }

    void UDPTalker::send(vector<uint8_t> outData, nanoseconds sendInterval, function<void(vector<uint8_t> inData, PVR_ERR error)> callback, nanoseconds timeout, bool waitToFinish) {
        error = PVR_ERR_NONE;

        copy(outData.begin(), outData.end(), lastOutData.begin());

        if (timeout > seconds(0))
            bomb.reset(timeout, true);

        if (callback != nullptr)
            receive(callback, seconds(0));

        auto lambda = [=] { // instance variables are still accessed by reference
            io_service svc;
            udp::socket skt(svc);
            skt.open(udp::v4());
            udp::endpoint remRcvEP;
            if (!ip.empty())
                remRcvEP = {address::from_string(ip), port};
            else {// broadcast if ip is unset
                skt.set_option(socket_base::broadcast(true));
                remRcvEP = {address_v4::broadcast(), port};
            }

            while (error != PVR_ERR_UNKNOWN || (callback == nullptr && error == PVR_ERR_UDP_TIMEOUT)) { // if receiving is active, wait until callback is executed
                skt.send_to(buffer(lastOutData), remRcvEP);
                sleep_for(sendInterval);
            }
            bomb.abort_async();
        };

        if (!waitToFinish)
            thr(lambda).detach(); // let the thread live outside the function scope
        else
            lambda();
    }
}
