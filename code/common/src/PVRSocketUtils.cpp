#include <iostream>
#include "PVRSocketUtils.h"

using namespace std;
using namespace std::this_thread;
using namespace asio;
using namespace asio::ip;
using namespace std::chrono;


//packetize TCP stream: "pvr" / msg type byte / data size (2 bytes) / data
// prefix -> 6 bytes

TCPTalker::TCPTalker(uint16_t port, function<void(PVR_MSG, vector<uint8_t>)> recCb,
	function<void(std::error_code)> errCb, bool isServer, string ip) {
	bool initted = false;
	thr = new std::thread([=, &initted] {
		try {
			io_service svc;
			tcp::socket skt(svc);
			_skt = &skt;
			tcp::acceptor acc(svc, {tcp::v4(), port});

			asio::error_code ec = asio::error::fault;
			auto errHdl = [&](const asio::error_code &err) {
				ec = err;
			};
			if (isServer) { // talker is a server
				if (ip.empty())
					acc.async_accept(skt, errHdl); // todo: simplify
				else
					skt.async_connect({address::from_string(ip), port}, errHdl);
				initted = true; //early unblock server
			} else            // talker is a client
				skt.async_connect({address::from_string(ip), port}, errHdl);
			svc.run();
			svc.reset();
			if (!isServer)
				initted = true; // on server mode this goes out of scope

			if (ec.value() == 0) {
				IP = skt.remote_endpoint().address().to_string();

				const size_t bufsz = 256;
				string prefix = "pvr";
				uint8_t buf[bufsz];
				vector<uint8_t> stream;
				size_t msgLen = 0;

				function<void(const asio::error_code &, size_t)> handle = [&](
						const asio::error_code &/*err*/, size_t len) {
					stream.insert(stream.end(), buf, buf + len);
					bool loop = true;
					while (loop) {
						auto it = find_first_of(stream.begin(), stream.end(), prefix.begin(),
												prefix.end());
						if (stream.end() - it >= 6) {
							msgLen = size_t(*(it + 4)) + size_t(*(it + 5)) * 0x100;
							it += 6; // skip prefix
							if (stream.size() >= it - stream.begin() +
												 msgLen) { // warning! do not increment iterator out of bounds
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
				svc.run();
			}
			if (ec.value() != 0)
				errCb(ec);

			sktMtx.lock();
			skt.close();
			_skt = nullptr;
			sktMtx.unlock();
		}
		catch(exception &e)
		{
			cerr << e.what() << endl;
		}
	});
	while (!initted)
		sleep_for(milliseconds(2));
}

void TCPTalker::safeDispatch(function<void()> hdl) {
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
}

bool TCPTalker::send(PVR_MSG msgType, vector<uint8_t> data) {
	vector<uint8_t> buf = { 'p', 'v', 'r', (uint8_t)msgType, 0, 0 };
	auto sz = data.size();
	memcpy(&buf[4], &sz, 2);
	buf.insert(buf.end(), data.begin(), data.end());

	bool success = false;
	safeDispatch([&] {
		if (_skt->is_open()) {
			asio::error_code ec;
			write(*_skt, buffer(buf), ec);
			success = ec.value() == 0;
		}
		else {
		}
	});
	return success;
}

TCPTalker::~TCPTalker() {
	safeDispatch([=] {
		_skt->get_io_service().stop();
	});
	EndThread(thr);
}