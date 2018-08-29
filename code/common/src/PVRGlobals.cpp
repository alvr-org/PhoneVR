#include "PVRGlobals.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace std;
using namespace std::chrono;


PVR_STATE pvrState = PVR_STATE_IDLE;


#define EXPIRE_DAYS 90
bool PVRCheckIfExpired() {
#if 0
	typedef system_clock sClk;
	tm tm = {};
	stringstream ss(XorString(__DATE__));
	ss >> get_time(&tm, XorString("%b %d %Y"));

	auto tp = sClk::from_time_t(mktime(&tm));
	if (duration_cast<hours>(sClk::now() - tp).count() / 24 > EXPIRE_DAYS)
		expiredCb(); // expired
#endif
	return false;
}

//void timeStart() {
//    _time = Clk::now();
//}
//
//void printTime() {
//    printf("%i\n", (int)duration_cast<milliseconds>(Clk::now() - _time).count());
//}

//////// only windows /////////
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {
	int64_t pvrdebug_oldMs = 0;
	const wstring logFile = L"C:\\Program Files\\PhoneVR\\pvrlog.txt";
}

void pvrdebug(string msg) {
	SYSTEMTIME t;
	GetLocalTime(&t);
	ofstream of(logFile, ios_base::app);
	auto ms = system_clock::now().time_since_epoch() / 1ms;
	string elapsed = (ms - pvrdebug_oldMs < 1000 ? to_string(ms - pvrdebug_oldMs) : "max");
	of << t.wMinute << ":" << setfill('0') << setw(2) << t.wSecond << " " << setfill('0') << setw(3) << elapsed << "  " << msg << endl;
	pvrdebug_oldMs = ms;
}

void pvrdebugClear() {
	ofstream(logFile, ofstream::out | ofstream::trunc);
}
#endif

//////// only android //////
#ifdef __ANDROID__

#include <android/log.h>

void pvrdebug(string msg) {
	__android_log_print(ANDROID_LOG_DEBUG, "pvr debugDef", "%s", msg.c_str());
}

#endif
