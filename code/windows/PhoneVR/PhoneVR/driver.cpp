#include "openvr_driver.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <set>

#include "PVRSockets.h"
#include "PVRGraphics.h"
#include "PVRFileManager.h"
#include "PVRMath.h"

using namespace std;
using namespace std::this_thread;
using namespace std::chrono;
using namespace Eigen;
using namespace vr;

namespace {
}

class HMD : public ITrackedDeviceServerDriver, public IVRDisplayComponent, public IVRVirtualDisplay { // todo: try to remove IVRDisplayComponent
	
	float projRect[4]; // left eye viewport, flip 
	float poseTmOffS;

	microseconds vstreamDT;

	PropertyContainerHandle_t propCont;

	string devIP;
	uint32_t objId = k_unTrackedDeviceIndexInvalid;

	Quaternionf latestQuat, newFrameQuat;

	Clk::time_point oldTime = Clk::now();
	uint64_t frmCount = 0;
	bool waitForPresent = false;

	TCPTalker talker;
	unique_ptr<TimeBomb> addDataBomb;

	DriverPose_t pose = {};

	//std::mutex mxaddDataRcvd;
	bool addDataRcvd = false;
	float ipd = 0.0;
	uint16_t rdrW = 0, rdrH = 0;

	void terminate() {
		//system("taskkill /f /im vrmonitor.exe"); // trigger closing
		//VREvent_Reserved_t data = { 0, 0 };
		//VREvent_Data_t
		VRServerDriverHost()->VendorSpecificEvent(objId, VREvent_DriverRequestedQuit, { 0, 0 }, 0);
	}

public:
	HMD(string ip) : talker(PVRProp<uint16_t>({ CONN_PORT_KEY }), [=](auto msgType, auto data)
	{
		PVR_DB_I("[HMD::talker]: recvd MSG_ID: " + to_string(msgType));
		if (msgType == PVR_MSG::DISCONNECT) {
			// TODO: send remove device event
			PVR_DB_I("[HMD::talker]: phone disconnected, closing server");
			terminate();
		}
		else if (msgType == PVR_MSG::ADDITIONAL_DATA) {
			PVR_DB_I("[HMD::talker]: addData msg RCV'ed....");

			//mxaddDataRcvd.lock();
			if (addDataRcvd)
			{
				PVR_DB_I("[HMD::talker]: addData is already Recvd and Set...skipping...");
				return;
			}
			//mxaddDataRcvd.unlock();
			//auto oldTm = Clk::now();
			//while (objId == k_unTrackedDeviceIndexInvalid && Clk::now() - oldTime < 3s) // 7s - ensure propCont is set
			//	sleep_for(10ms);

			//PVR_DB_I("[HMD::talker]: ADD DATA MSG RCV'ed.. propCont seems to be set or timeout");
			//if (objId == k_unTrackedDeviceIndexInvalid)
			{
				auto ui16Data = reinterpret_cast<uint16_t *>(&data[0]);
				rdrW = ui16Data[0];
				rdrH = ui16Data[1];
				memcpy(projRect, &data[2 * 2], 4 * 4);
				PVR_DB_I("[HMD::talker]: addData: Viewport:  left: " + to_string(projRect[0]) + "  top: " + to_string(projRect[1]) +
					"  right: " + to_string(projRect[2]) + "  bottom: " + to_string(projRect[3]));
				ipd = *reinterpret_cast<float *>(&data[2 * 2 + 4 * 4]);

				//VRProperties()->SetFloatProperty(propCont, Prop_UserIpdMeters_Float, ipd);
				PVR_DB_I("[HMD::talker]: IPD: " + to_string(ipd));

				//mxaddDataRcvd.lock();
				addDataRcvd = true;
				//mxaddDataRcvd.unlock();

				//addDataBomb->defuse();
			}

		}
	}, [=](error_code err)
	{
		PVR_DB_I("[HMD::talker]: TCP error: " + err.message());
		if (err.value() == 104) {
			terminate();
		}
	}, false, ip), devIP(ip)
	{

		PVR_DB_I("HMD initializing");

		pose.poseTimeOffset = 0;
		pose.qWorldFromDriverRotation = { 1.0, 0.0, 0.0, 0.0 };
		pose.qDriverFromHeadRotation = { 1.0, 0.0, 0.0, 0.0 };
		pose.qRotation = { 1.0, 0.0, 0.0, 0.0 };
		pose.result = TrackingResult_Running_OK;
		pose.poseIsValid = true;
		pose.willDriftInYaw = false;
		pose.shouldApplyHeadModel = false;
		pose.deviceIsConnected = true;


		vstreamDT = 1'000'000us / PVRProp<int>({ GAME_FPS_KEY });

		PVR_DB_I("HMD Sending PAIR_ACCEPT TCP msg to " + ip);
		talker.send(PVR_MSG::PAIR_ACCEPT);

		PVRInitDX();

		PVR_DB_I("HMD initialized");
	}

	virtual ~HMD() {
		PVR_DB_I("HMD destroying");
		//PVRCloseStreamer();
		PVRReleaseDX();
		talker.send(PVR_MSG::DISCONNECT);
		PVR_DB_I("HMD destroyed");
	}

	virtual DriverPose_t GetPose() override {
		return pose;
	}

	virtual EVRInitError Activate(uint32_t objectId) override {
		PVR_DB_I("[Activating HMD]... Setting Props");

		// set properties
		propCont = VRProperties()->TrackedDeviceToPropertyContainer(objectId);

		VRProperties()->SetStringProperty(propCont, Prop_ModelNumber_String, "PVRServer");
		VRProperties()->SetStringProperty(propCont, Prop_RenderModelName_String, "PVRServer");

		//VRProperties()->SetBoolProperty(propCont, Prop_WillDriftInYaw_Bool, true);
		VRProperties()->SetStringProperty(propCont, Prop_ManufacturerName_String, "Viritualis Res");
		VRProperties()->SetBoolProperty(propCont, Prop_DeviceIsWireless_Bool, true);
		VRProperties()->SetBoolProperty(propCont, Prop_DeviceIsCharging_Bool, false);
		VRProperties()->SetFloatProperty(propCont, Prop_DeviceBatteryPercentage_Float, 1); //TODO get from phone
		VRProperties()->SetBoolProperty(propCont, Prop_Firmware_UpdateAvailable_Bool, false);//TODO implement
		VRProperties()->SetBoolProperty(propCont, Prop_Firmware_ManualUpdate_Bool, true);
		VRProperties()->SetBoolProperty(propCont, Prop_BlockServerShutdown_Bool, false);
		VRProperties()->SetBoolProperty(propCont, Prop_CanUnifyCoordinateSystemWithHmd_Bool, true);
		VRProperties()->SetBoolProperty(propCont, Prop_ContainsProximitySensor_Bool, false);
		VRProperties()->SetBoolProperty(propCont, Prop_DeviceProvidesBatteryStatus_Bool, false); //TODO implement and then set to true
		VRProperties()->SetBoolProperty(propCont, Prop_DeviceCanPowerOff_Bool, true);
		//VRProperties()->SetInt32Property(propCont, Prop_DeviceClass_Int32, TrackedDeviceClass_HMD);
		VRProperties()->SetBoolProperty(propCont, Prop_HasCamera_Bool, false);
		VRProperties()->SetStringProperty(propCont, Prop_DriverVersion_String, (to_string(PVR_SERVER_VERSION >> 24) + "." + to_string((PVR_SERVER_VERSION >> 16) % 0x100)).c_str());
		VRProperties()->SetBoolProperty(propCont, Prop_Firmware_ForceUpdateRequired_Bool, false); //TODO implement
		//VRProperties()->SetBoolProperty(propCont, Prop_ViveSystemButtonFixRequired_Bool, false); // ??
		VRProperties()->SetBoolProperty(propCont, Prop_ReportsTimeSinceVSync_Bool, false);
		VRProperties()->SetFloatProperty(propCont, Prop_SecondsFromVsyncToPhotons_Float, 0.100f);
		VRProperties()->SetFloatProperty(propCont, Prop_DisplayFrequency_Float, PVRProp<float>({ GAME_FPS_KEY }));
		VRProperties()->SetUint64Property(propCont, Prop_CurrentUniverseId_Uint64, /*P+V+R ascii = */ 0x808682);
		VRProperties()->SetBoolProperty(propCont, Prop_IsOnDesktop_Bool, false);
		VRProperties()->SetFloatProperty(propCont, Prop_UserHeadToEyeDepthMeters_Float, 0.0f);
		//VRProperties()->SetBoolProperty(propCont, Prop_DisplaySuppressed_Bool, false); // ??    false
		VRProperties()->SetBoolProperty(propCont, Prop_DisplayAllowNightMode_Bool, true); // ??

		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceOff_String, "{PVRServer}/icons/hmd_off.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceSearching_String, "{PVRServer}/icons/hmd_alert.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceSearchingAlert_String, "{PVRServer}/icons/hmd_alert.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceReady_String, "{PVRServer}/icons/hmd_ready.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceReadyAlert_String, "{PVRServer}/icons/hmd_ready_alert.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceNotReady_String, "{PVRServer}/icons/hmd_error.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceStandby_String, "{PVRServer}/icons/hmd_standby.png");
		VRProperties()->SetStringProperty(propCont, Prop_NamedIconPathDeviceAlertLow_String, "{PVRServer}/icons/hmd_ready_low.png");

		//bool addDataRcvd = false;
		//addDataBomb.reset(new TimeBomb(5s, [&] { addDataRcvd = false; }));
		//addDataBomb->ignite(false);
		
		objId = objectId;
		auto oldTime = Clk::now();

		PVR_DB_I("[Activating HMD]: waiting for additionalData from Client...");
		auto timeout = (seconds)(PVRProp<int>({ CONN_TIMEOUT }));

		while ( (Clk::now() - oldTime < timeout) )
		{
			//mxaddDataRcvd.lock();
			if (addDataRcvd) break;
			//mxaddDataRcvd.unlock();

			//PVR_DB("[Activating HMD]: waiting for additionalData from Client...");
			//sleep_for(50us);
		}

		if (addDataRcvd) 
		{
			PVR_DB_I("[Activating HMD]: Rcvd addotionalData... Starting PVRStartStreamer with [WxH]:" + to_string(rdrW) + "x" + to_string(rdrH));

			VRProperties()->SetFloatProperty(propCont, Prop_UserIpdMeters_Float, ipd);
			
			PVRStartStreamer(devIP, rdrW, rdrH, [=](auto v) {
				talker.send(PVR_MSG::HEADER_NALS, v);
			}, [=] { terminate(); });

			PVRStartReceiveData(devIP, &pose, &objId);

			PVR_DB_I("[Activating HMD]: HMD activated with id: " + to_string(objectId));

			return VRInitError_None;
		}
		else {
			PVR_DB_I("[Activating HMD]: Timeout didnt recv additionalData... for " + to_string(timeout.count()) + "secs");
			return VRInitError_Unknown;
		}
	}

	virtual void Deactivate() override {
		PVR_DB_I("HMD deactivating");

		rdrW = 0;
		rdrH = 0;
		objId = k_unTrackedDeviceIndexInvalid;

		PVR_DB_I("Closing data thread");
		PVRStopReceiveData();

		PVR_DB_I("Closing video thread");
		PVRStopStreamer();

		PVR_DB_I("HMD deactivated");
	}

	virtual void *GetComponent(const char *compNameAndVersion) override {
		//PVR_DB("Getting component: "s + compNameAndVersion);
		if (!_stricmp(compNameAndVersion, IVRDisplayComponent_Version))
			return (IVRDisplayComponent *)this;
		else if (!_stricmp(compNameAndVersion, IVRVirtualDisplay_Version))
			return (IVRVirtualDisplay *)this;

		return nullptr;
	}

	virtual void DebugRequest(const char * /*pchRequest*/, char *pchResponseBuffer, uint32_t unResponseBufferSize) override {
		PVR_DB_I("debug request");
		if (unResponseBufferSize > 0)
			pchResponseBuffer[0] = 0;
	}


	virtual void EnterStandby() override {
		PVR_DB_I("[Open VR EnterStandby] HMD enter standby");
	}

	// IVRDisplayComponent methods

	virtual void GetWindowBounds(int32_t *x, int32_t *y, uint32_t *width, uint32_t *height) override {

		PVR_DB_I("[Open VR GetWindowBounds] Returning w " + to_string(*width)+ " h " + to_string(*height));
		*x = 0;
		*y = 0;
		*width = rdrW;
		*height = rdrH;
	}

	virtual bool IsDisplayOnDesktop() override {
		return false;
	}

	virtual bool IsDisplayRealDisplay() override {
		return false;
	}

	virtual void GetRecommendedRenderTargetSize(uint32_t *width, uint32_t *height) override {

		PVR_DB_I("[Open VR GetRecommendedRenderTargetSize] Returning w " + to_string(*width) + " h " + to_string(*height));
		*width = rdrW;
		*height = rdrH;
	}

	virtual void GetEyeOutputViewport(EVREye eye, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height) override {
		*width = rdrW / 2;
		*height = rdrH;
		*x = rdrW / 2 * eye;
		*y = 0;
	}

	virtual void GetProjectionRaw(EVREye eye, float *left, float *right, float *top, float *bottom) override {
		//flip left-right for right eye
		*left = eye == Eye_Left ? projRect[0] : -projRect[2];
		*top = projRect[3];
		*right = eye == Eye_Left ? projRect[2] : -projRect[0];
		*bottom = projRect[1];
	}

	virtual DistortionCoordinates_t ComputeDistortion(EVREye /*eye*/, float u, float v) override {
		DistortionCoordinates_t coord;
		coord.rfBlue[0] = u;
		coord.rfBlue[1] = v;
		coord.rfGreen[0] = u;
		coord.rfGreen[1] = v;
		coord.rfRed[0] = u;
		coord.rfRed[1] = v;
		return coord;
	}

	// logic to get the most recent orientation quaternion associated with the frame
	virtual void Present(SharedTextureHandle_t backBuffer) override {
		//PVR_DB("present1");
		//waitForPresent = true;
		frmCount++;
		//postPEQ.enqueue(*outQuat, (float)deltaNs / 1'000'000'000.f);

		while (Clk::now() - oldTime < vstreamDT)
			sleep_for(500us);

		auto now = Clk::now();
		//if (now - oldTime > vstreamDT) {
		PVRProcessFrame(backBuffer, newFrameQuat);
		oldTime = now;

		//}
		while (Clk::now() - oldTime < vstreamDT - 1ms)
			sleep_for(1ms);

		//newFrameQuat = Quaternionf(latestQuat); // poll here the quaternion used in the next frame
		//VRServerDriverHost()->TrackedDevicePoseUpdated(objId, GetPose(), sizeof(DriverPose_t));
		//waitForPresent = false;
	//VRServerDriverHost()->VsyncEvent(0.0028);
	}

	/** Block until the last presented buffer start scanning out. */
	virtual void WaitForPresent() override {
		//while (Clk::now() - oldTime < vstreamDT)
			//sleep_for(1ms);
	}

	/** Provides timing data for synchronizing with display. */
	virtual bool GetTimeSinceLastVsync(float *secSinceLastSync, uint64_t *frmCnt) override {
		//PVR_DB("get time");
		//*frmCnt = frmCount;
		//*secSinceLastSync = 0.005f; // could not determine
		return false;
	}

};


class ServerProvider : public IServerTrackedDeviceProvider {
	HMD *hmd = nullptr;
	set<string> IPs;
	vector<ITrackedDeviceServerDriver *> ctrls;

public:
	virtual EVRInitError Init(IVRDriverContext *drvCtx) override {
		PVR_DB_I("[ServerProvider::Init] Initializing server");

		bool hmdPaired = false;
		auto timeout = (seconds)(PVRProp<int>({ CONN_TIMEOUT }));

		TimeBomb hmdBomb(timeout);

		// any exception is handled by the called method
		// Start Listening for any incoming connection on CONN_PORT(def:33333)
		PVRStartConnectionListener([&](auto ip, auto msgType) {
			if (msgType == PVR_MSG::PAIR_HMD && !hmd) {
				//VR_INIT_SERVER_DRIVER_CONTEXT(drvCtx);

				//#define VR_INIT_SERVER_DRIVER_CONTEXT( pContext ) 
				//		{ 
				vr::EVRInitError eError = vr::InitServerDriverContext( drvCtx );
				PVR_DB_I("[ServerProvider::Init::PVRStartConnectionListener] recvd PAIR_HMD Msg from ip: " + to_string(ip));
				if (eError == vr::VRInitError_None)
				{
					hmd = new HMD(ip);
					//red = new Redirect();
					hmdPaired = VRServerDriverHost()->TrackedDeviceAdded("0", TrackedDeviceClass_HMD, hmd);
					//hmdPaired = VRServerDriverHost()->TrackedDeviceAdded("1", TrackedDeviceClass_DisplayRedirect, red);
					if (!hmdPaired)
					{
						PVR_DB_I("[ServerProvider::Init::PVRStartConnectionListener] Error: could not activate HMD");
					}
					else
						hmdBomb.defuse();
				}
				else
				{
					PVR_DB_I("[ServerProvider::Init::PVRStartConnectionListener] Error: could not InitServerDriverContext");
				}
			}
			else if (msgType == PVR_MSG::PAIR_PHONE_CTRL) {
				if (find(IPs.begin(), IPs.end(), ip) == IPs.end()) {
					IPs.insert(ip);
					PVR_DB_I("[ServerProvider::Init::PVRStartConnectionListener] Recvd PAIR_PHONE_CTRL from ip: " + to_string(ip));
					// init controller
					//VRServerDriverHost()->TrackedDeviceAdded(to_string(ctrls.size()).c_str(), TrackedDeviceClass_Controller, ctrl);
					//ctrls.insert(ctrl)
				}
			}
		});

		hmdBomb.ignite(false);

		if (!hmdPaired) {
			PVRStopConnectionListener();
			PVR_DB_I("[ServerProvider::Init] Pairing failed! Cause: timeout ... for " + to_string(timeout.count()) + "secs");
			return VRInitError_Init_HmdNotFound;
		}

		PVR_DB_I("[ServerProvider::Init] Server initialized");
		return VRInitError_None;
	}

	virtual void Cleanup() override {
		PVRStopConnectionListener();

		for (auto ctrl : ctrls) {
			delete ctrl;
		}

		delete hmd;
		hmd = nullptr;

		VR_CLEANUP_SERVER_DRIVER_CONTEXT();

		PVR_DB_I("Server cleanup");
	}

	virtual const char *const *GetInterfaceVersions() override { return k_InterfaceVersions; }

	virtual void RunFrame() override {
		//#if DEBUG_NO_DEV
		//        if (hmd && hmd->objId != k_unTrackedDeviceIndexInvalid) {
		//            VRServerDriverHost()->TrackedDevicePoseUpdated(hmd->objId, hmd->GetPose(), sizeof(DriverPose_t));
		//        }
		//#endif
	}

	virtual bool ShouldBlockStandbyMode() override {
		//PVR_DB("Block standby mode: false");
		return false;
	}

	virtual void EnterStandby() override {
		PVR_DB_I("Server enter standby");
	}

	virtual void LeaveStandby() override {
		PVR_DB_I("Server leave standby");
	}
};

static ServerProvider server;

extern "C" __declspec(dllexport)
void *HmdDriverFactory(const char *iName, int *retCode) {
	/*if (PVRCheckIfExpired()) {
		PVR_DB("This version of PhoneVR driver has expired. Please install a more recent version");
		*retCode = VRInitError_Driver_Failed;
		return nullptr;
	}*/
	if (!PVRProp<bool>({ ENABLE_KEY })) {
		PVR_DB_I("PhoneVR server disabled!");
		*retCode = VRInitError_Driver_Failed;
		return nullptr;
	}

	if (strcmp(iName, IServerTrackedDeviceProvider_Version) == 0) {
		//PVR_DB_CLEAR();
		PVR_DB_I("------------------------------------");
		PVR_DB_I("Getting server");
		return &server;
	}
	if (strcmp(iName, IVRWatchdogProvider_Version) == 0) {
		PVR_DB_I("Watchdog not supported");
		return nullptr;
	}
	PVR_DB_I("Interface not found: "s + iName);
	if (retCode)
		*retCode = VRInitError_Init_InterfaceNotFound;
	return nullptr;
}