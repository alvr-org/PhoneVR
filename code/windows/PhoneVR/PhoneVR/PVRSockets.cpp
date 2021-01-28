#include "PVRSockets.h"

#include <fstream>
#include <chrono>
#include <queue>

#include "PVRSocketUtils.h"
#include "PVRGraphics.h"
#include "PVRMath.h"
#include "PVRFileManager.h"

extern "C" {
#include "x264.h"
}

#pragma comment(lib, "x264.lib")

using namespace std;
using namespace std::placeholders;
using namespace std::this_thread;
using namespace std::chrono;
using namespace asio;
using namespace asio::ip;
using namespace Eigen;

namespace {
	const int FMT = X264_CSP_I420;
	const size_t nVFrames = 5;

	std::thread *connThr, *videoThr, *dataThr;
	io_service *connSvc, *dataSvc;
	bool connRunning, videoRunning = false, dataRunning;

	queue<
		pair<
			pair<
				int64_t /*pts*/,				//pts
				pair<
					Clk::time_point,	//tpWhenFrameRecvd from OpenVRSDK::Present
					float
				>						//Renderer Delay(ms)
			>, 
			Quaternionf 
		>> quatQueue;

	vector<x264_picture_t> vFrames(nVFrames);
	int64_t pts = 0;         // in microseconds
	int64_t vFrameDtUs;
	int whichFrame = 0;      // syncronize rendering and encoding
	std::mutex whichFrameMtxs[nVFrames]; // if syncronization is out (cause lag) use as last resort to avoid crashing
	std::mutex quatQueueMutex; // Syncronization of quatQueue among SteamVR Thread and Streamer thread.

	float fpsSteamVRApp = 0.0;
	float fpsStreamer = 0.0;
	float fpsStreamWriter = 0.0;
	float fpsEncoder = 0.0;
}

float fpsRenderer = 0.0;

//prePEQ.enqueue(Quaternionf(quatBuf[0], quatBuf[1], quatBuf[2], quatBuf[3]),
   // (float)(tmBuf[0] - lastNanos) / 1'000'000'000.f);// difference first -> avoid losing accuracy
//lastNanos = tmBuf[0];

//Quaternionf quat = prePEQ.getQuatIn(latency);


// pair packet: "pvr" / msgType byte / app version (uint)

// Invoked on ServerProviderInit() to Listen for any incoming connection on CONN_PORT (def:33333)
void PVRStartConnectionListener(function<void(string, PVR_MSG)> callback) {
	connRunning = true;
	connThr = new std::thread([=] {
		try
		{
			io_service svc;
			connSvc = &svc;
			udp::socket skt(svc, { udp::v4(), PVRProp<uint16_t>({ CONN_PORT_KEY }) });
			udp::endpoint remEP;

			uint8_t buf[256];
			while (connRunning) {
				function<void(const asio::error_code &, size_t)> handle = [&](auto err, auto pktSz) {
					if (err.value())
					{
						PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Error listening (" + to_string(err.value()) + "): " + err.message());
					}
					//PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Recvd data... Interpreting...");

					if (pktSz == 8 && string(buf, buf + 3) == "pvr") {
						auto ip = remEP.address().to_string();
						auto msgType = (PVR_MSG)buf[3];
						if (msgType == PVR_MSG::PAIR_HMD || msgType == PVR_MSG::PAIR_PHONE_CTRL) {
							if (vec2uint(&buf[4]) >= PVR_CLIENT_VERSION)
								callback(ip, msgType);
							else
								PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Device " + ip + " needs to be updated");
						}
						else
							PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Invalid message from device: " + ip);
						buf[0] = 0;
						skt.async_receive_from(buffer(buf), remEP, handle);
					}
				};
				PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Listener Started on Port : " + to_string(PVRProp<uint16_t>({ CONN_PORT_KEY })));
				skt.async_receive_from(buffer(buf), remEP, handle);
				svc.run();
				svc.reset();
			}
		}
		catch (const std::system_error& err)
		{
			PVR_DB_I("[PVRSockets::PVRStartConnectionListener] Caught exception: " + string(err.what()));
		}
	});
}

void PVRStopConnectionListener() {
	connRunning = false;
	connSvc->stop(); // todo use mutex
	EndThread(connThr);
}


void PVRStartStreamer(string ip, uint16_t width, uint16_t height, function<void(vector<uint8_t>)> headerCb, function<void()> onErrCb) {
	videoRunning = true;
	videoThr = new std::thread([=] {
		PVR_DB("[PVRStartStreamer th] Setting encoder");
		auto S = ENCODER_SECT;
		int fps = PVRProp<int>({ GAME_FPS_KEY });

		//auto wait = 1'000'000us / fps / 5;
		vFrameDtUs = (1'000'000us / fps).count();

		x264_param_t par;
		x264_picture_t outPic;
		auto res = x264_param_default_preset(&par, PVRProp<string>({ S, PRESET_KEY }).c_str(), PVRProp<string>({ S, TUNE_KEY }).c_str());

		{
			par.i_csp = FMT;
			par.i_width = width;
			par.i_height = height;
			par.b_vfr_input = 0; // disable variable frame rate, ignore pts
			par.b_repeat_headers = 1;
			par.b_annexb = 1;
			par.i_fps_num = fps;

			par.rc.i_rc_method = PVRProp<int>({ S, RC_METHOD_KEY }); // 0->X264_RC_CQP, 2->X264_RC_ABR;

			auto qcomp = PVRProp<float>({ S, QCOMP_KEY });
			if (qcomp >= 0)
				par.rc.f_qcompress = qcomp;

			auto qp = PVRProp<int>({ S, QP_KEY });
			if (qp > 0) {
				par.rc.i_qp_constant = qp;
				par.rc.i_qp_min = qp - 5;
				par.rc.i_qp_max = qp + 5;
			}


			auto br = PVRProp<int>({ S, BITRATE_KEY });
			if (br > 0)
				par.rc.i_bitrate = br;

			auto ki_max = PVRProp<int>({ S, KEYINT_MAX_KEY });
			if (ki_max > 0)
				par.i_keyint_max = ki_max;

			par.b_intra_refresh = PVRProp<bool>({ S, I_REFRESH_KEY }) ? 1 : 0;
		}
		res = x264_param_apply_profile(&par, PVRProp<string>({ S, PROFILE_KEY }).c_str());



		par.rc.i_rc_method = 1;

		par.rc.i_bitrate = 1000;
		par.rc.i_vbv_max_bitrate = 1200;
		par.rc.i_vbv_buffer_size = 20000;
		par.rc.f_vbv_buffer_init = 0.9f;

		//par.rc.i_qp_constant = 25;
		//par.rc.i_qp_min = par.rc.i_qp_constant - 1;
		//par.rc.i_qp_max = par.rc.i_qp_constant + 1;
		//par.rc.f_qcompress = 0.0f;

		//par.rc.f_rf_constant = 12;
		//par.rc.f_rf_constant_max = 13;
		par.rc.f_rf_constant = 24;
		par.rc.f_rf_constant_max = 26;



		//par.nalu_process           TODO: callback available!!!!!!!!! manage a udp thread inside here, then dispatch sends
		// use opaque pointer to know from which frame a nal belongs
		// use i_first_mb to sort slice nals
		// still need to find out how to sort non-slice nals

		vector<vector<uint8_t *>> vvbuf;
		for (size_t i = 0; i < nVFrames; i++) {
			x264_picture_alloc(&vFrames[i], FMT, width, height);
			vvbuf.push_back({ vFrames[i].img.plane[0], vFrames[i].img.plane[1], vFrames[i].img.plane[2] });
		}
		PVRStartGraphics(vvbuf, width, height);


		auto *enc = x264_encoder_open(&par);

		x264_param_t outPar;
		x264_encoder_parameters(enc, &outPar);
		PVR_DB("[PVRStartStreamer th] Render size: " + to_string(width) + "x" + to_string(height));
		PVR_DB("[PVRStartStreamer th] Using encoding level: " + to_string(outPar.i_level_idc));

		x264_nal_t *nals;
		int nNals;
		res = x264_encoder_headers(enc, &nals, &nNals);
		vector<uint8_t> vheader;
		for (size_t i = 0; i < nNals; i++)
			vheader.insert(vheader.end(), nals[i].p_payload, nals[i].p_payload + nals[i].i_payload);  // WARNING: including SEI nal
		headerCb(vheader);

		io_service svc;
		tcp::socket skt(svc);
		tcp::acceptor acc(svc, { tcp::v4(), PVRProp<uint16_t>({ VIDEO_PORT_KEY }) });
		acc.accept(skt);

		//udp::socket skt(svc);
		//udp::endpoint remEP(address::from_string(ip), port);
		//skt.open(udp::v4());

		int lastWhichFrame = 0;
		//uint8_t buf[256 * 256];
		uint8_t extraBuf[8 + 16 + 4 + 20 + 8 + 8];
		auto pbuf = reinterpret_cast<int64_t *>(&extraBuf[0]); // pts buf ref
		auto qbuf = reinterpret_cast<float *>(&extraBuf[8]); // quat buf ref
		auto nbuf = reinterpret_cast<int *>(&extraBuf[8 + 16]);
		auto fpsbuf = reinterpret_cast<float *>(&extraBuf[8 + 16 + 4]);
		auto tDelaysBuf = reinterpret_cast<float *>(&extraBuf[8 + 16 + 4 + 20]);
		auto timestamp = reinterpret_cast<int64_t *>(&extraBuf[8 + 16 + 4 + 20 + 8]);

		asio::error_code ec;
		//ofstream outp("C:\\Users\\narni\\mystream.h264", ofstream::binary);/////////////////////////////////////////////
		while ((whichFrame == lastWhichFrame || quatQueue.size() == 0) && videoRunning) // for first frame
			sleep_for(500us);
		while (videoRunning)
		{
			//PVRUpdTexWraps();
			static Clk::time_point oldtime, oldtimeStreamer;
			oldtime = Clk::now();
			oldtimeStreamer = Clk::now();

			if ((lastWhichFrame + 1) % nVFrames != whichFrame)
				PVR_DB_I("[PVRStartStreamer th] Skipped frame! Please re-tune the encoder parameters lWf:"+to_string(lastWhichFrame)+", nVfs:"+ to_string(nVFrames)+ ", wF:"+to_string(whichFrame));
			lastWhichFrame = whichFrame;
			whichFrameMtxs[lastWhichFrame].lock();   // LOCK
			auto totSz = x264_encoder_encode(enc, &nals, &nNals, &vFrames[lastWhichFrame], &outPic);
			whichFrameMtxs[lastWhichFrame].unlock(); // UNLOCK

			fpsEncoder = (1000000000.0 / (Clk::now() - oldtime).count());

			oldtime = Clk::now();

			if (totSz > 0)
			{
				PVR_DB("[PVRStartStreamer th] Rendering lWf:"+to_string(lastWhichFrame)+", wF:"+to_string(whichFrame));
				quatQueueMutex.lock();   // LOCK
				while ((quatQueue.size() != 0) && (quatQueue.front().first.first < outPic.i_pts)) // handle skipped frames
				{
					PVR_DB("[PVRStartStreamer th] handle skipped frames qPts:" + to_string(quatQueue.front().first.first) + ", outpicPts:" + to_string(outPic.i_pts));
					quatQueue.pop();
				}
				quatQueueMutex.unlock(); // UNLOCK

				if (quatQueue.size() != 0)
				{
					quatQueueMutex.lock();
					auto outPts = quatQueue.front().first.first;
					auto time = quatQueue.front().first.second.first;
					auto renderDur = quatQueue.front().first.second.second;
					auto quat = quatQueue.front().second;
					quatQueue.pop();
					quatQueueMutex.unlock();

					*pbuf = outPts;
					qbuf[0] = quat.w();
					qbuf[1] = quat.x();
					qbuf[2] = quat.y();
					qbuf[3] = quat.z();
					*nbuf = totSz;
					fpsbuf[0] = fpsSteamVRApp;		// VRApp FPS
					fpsbuf[1] = fpsEncoder;			// Encoder FPS
					fpsbuf[2] = fpsStreamWriter;	// StreamWriter FPS
					fpsbuf[3] = fpsStreamer;		// Streamer FPS
					fpsbuf[4] = fpsRenderer;
					tDelaysBuf[0] = renderDur;		// Renderer Delay
					tDelaysBuf[1] = (float)((Clk::now() - time).count() / 1000000.0);	// Encoder Delay
					*timestamp = (int64_t)duration_cast<microseconds>(system_clock::now().time_since_epoch()).count(); // FrameSent TimeStamp

					write(skt, buffer(extraBuf), ec);
					write(skt, buffer(nals->p_payload, totSz), ec);

					PVR_DB("[PVRStartStreamer th] wrote render to socket: Pts:[Tenc:"
						+ str_fmt("%.2f", tDelaysBuf[1])
						+" ms, Trend:"
						+ str_fmt("%.2f", renderDur)
						+" ms]"
						+ to_string(outPts) + ", Size: "
						+to_string(sizeof(extraBuf))+","+to_string(totSz));

					if (ec.value() != 0 && videoRunning)
						PVR_DB("Write failed: " + ec.message() + " Code: " + to_string(ec.value()));
					if (ec == error::connection_aborted || ec == error::connection_reset)
					{
						videoRunning = false;
						//onErrCb();
					}

					//outp.write((char*)nals->p_payload, totSz);/////////////////////////////////////////////////////////
				}
			}
			fpsStreamWriter = (1000000000.0 / (Clk::now() - oldtime).count());
			PVR_DB("[PVRStartStreamer th] ------------------- StreamWriting @ FPS: " + to_string(fpsStreamWriter) + " Encoding @ FPS : " + to_string(fpsEncoder) + " VRApp Running @ FPS : " + to_string(fpsSteamVRApp));
			oldtime = Clk::now();

			while ((whichFrame == lastWhichFrame || quatQueue.size() == 0) && videoRunning)
				sleep_for(500us);

			fpsStreamer = (1000000000.0 / (Clk::now() - oldtimeStreamer).count());
			oldtimeStreamer = Clk::now();
		}
		x264_encoder_close(enc);

		PVRStopGraphics();

		//outp.close();//////////////////////////////////////////////////////////////////////////////////////////

		for (auto frame : vFrames)
			x264_picture_clean(&frame);
	});
}

void PVRProcessFrame(uint64_t hdl, Quaternionf quat) {
	if (videoRunning) {

		static Clk::time_point oldtimeVRApp = Clk::now();
		
		int newWhichFrame = (whichFrame + 1) % nVFrames;
		whichFrameMtxs[newWhichFrame].lock();   // LOCK

		pts += vFrameDtUs;

		quatQueueMutex.lock();	// LOCK
		quatQueue.push({ {pts, {Clk::now(),0}}, quat });
		quatQueueMutex.unlock(); // UNLOCK

		//PVRUpdTexHdl() -> Blocking: This will wait if there is already a handle being rendered.
		PVRUpdTexHdl(hdl, newWhichFrame); // Render Frame

		quatQueueMutex.lock();	// LOCK
		quatQueue.back().first.second.second = (Clk::now() - quatQueue.back().first.second.first).count() / 1000000.0;
		quatQueueMutex.unlock(); // UNLOCK

		vFrames[newWhichFrame].i_pts = pts;

		whichFrameMtxs[newWhichFrame].unlock(); // UNLOCK
		whichFrame = newWhichFrame;

		fpsSteamVRApp = (1000000000.0 / (Clk::now() - oldtimeVRApp).count());

		/*PVR_DB("[PVRProcessFrame] pushed frame to que Wf: " 
			+ to_string(whichFrame) 
			+ ", pts(Trend:"+ str_fmt("%.2f", (Clk::now() - quatQueue.back().first.second.first).count() / 1000000.0) + "ms): "
			+ to_string(pts));*/
		oldtimeVRApp = Clk::now();
	}
}


void PVRStopStreamer() {
	videoRunning = false;
	EndThread(videoThr);
}


void PVRStartReceiveData(string ip, vr::DriverPose_t *pose, uint32_t *objId) {
	float addLatency = 0;

	PVR_DB_I("[PVRStartReceiveData] UDP receive started");

	dataRunning = true;
	dataThr = new std::thread([=] {
		try
		{
			io_service svc;
			dataSvc = &svc;
			udp::socket skt(svc, { udp::v4(), PVRProp<uint16_t>({ POSE_PORT_KEY }) });

			uint8_t buf[256];
			auto quatBuf = reinterpret_cast<float *>(&buf[0]);
			//auto accBuf = reinterpret_cast<float *>(&buf[4 * 4]);
			auto tmBuf = reinterpret_cast<long long *>(&buf[4 * 4 + 3 * 4]);



			while (dataRunning) {
				function<void(const asio::error_code &, size_t)> handle = [&](auto, auto pktSz) {
					//PVR_DB(err.message());

					if (pktSz > 24) {
						//if (isValidOrient(quat))// check if quat is valid
						pose->qRotation = { quatBuf[0], quatBuf[1], quatBuf[2], quatBuf[3] }; // w x y z
						pose->poseTimeOffset = float(Clk::now().time_since_epoch().count() - *tmBuf) / 1'000'000'000.f + addLatency; // Clk and tmBuf is in nanoseconds
						vr::VRServerDriverHost()->TrackedDevicePoseUpdated(*objId, *pose, sizeof(vr::DriverPose_t));

						skt.async_receive(buffer(buf), handle);
					}
				};
				skt.async_receive(buffer(buf), handle);
				svc.run();
				svc.reset();
			}


			PVR_DB_I("UDP receive stopped");

		}
		catch (const std::system_error& err)
		{
			PVR_DB_I(err.what());
		}
		dataSvc = nullptr;
	});

}

void PVRStopReceiveData() {
	dataRunning = false;
	if (dataSvc)
		dataSvc->stop();
	EndThread(dataThr);
}