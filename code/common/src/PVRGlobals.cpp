#include "PVRGlobals.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace std;
using namespace std::chrono;


PVR_STATE pvrState = PVR_STATE_IDLE;


/*#define EXPIRE_DAYS 90
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
}*/

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
	int64_t pvrInfo_oldMs = 0;
	const wstring logFileDebug = L"C:\\Program Files\\PhoneVR\\pvrDebuglog.txt";
	const wstring logFileInfo = L"C:\\Program Files\\PhoneVR\\pvrlog.txt";
}

void pvrdebug(string msg) {
	SYSTEMTIME t;
	GetLocalTime(&t);
	ofstream of(logFileDebug, ios_base::app);
	auto ms = system_clock::now().time_since_epoch() / 1ms;
	string elapsed = (ms - pvrdebug_oldMs < 1000 ? to_string(ms - pvrdebug_oldMs) : "max");
	of << t.wMinute << ":" << setfill('0') << setw(2) << t.wSecond << " " << setfill('0') << setw(3) << elapsed << "  " << msg << endl;
	
	OutputDebugStringA( ("--[PhoneVR]--"+msg+"\n").c_str() );

	pvrdebug_oldMs = ms;	
}

void pvrdebugClear() {
	ofstream(logFileDebug, ofstream::out | ofstream::trunc);
}

void pvrInfo(string msg) {
	SYSTEMTIME t;
	GetLocalTime(&t);
	ofstream of(logFileInfo, ios_base::app);
	auto ms = system_clock::now().time_since_epoch() / 1ms;
	string elapsed = (ms - pvrInfo_oldMs < 1000 ? to_string(ms - pvrInfo_oldMs) : "max");
	of  << setfill('0') << setw(2) << t.wHour  << ":"
		<< setfill('0') << setw(2) << t.wMinute << ":"
		<< setfill('0') << setw(2) << t.wSecond << "("
		<< setfill('0') << setw(3) << t.wMilliseconds
		<< ") - " 
		<< setfill('0') << setw(2) << t.wDay << "."
		<< setfill('0') << setw(2) << t.wMonth << "."
		<< setfill('0') << setw(4) << t.wYear
		<< " -- " 
		<< setfill('0') << setw(2) << t.wSecond << " " 
		<< setfill('0') << setw(3) << elapsed << "  " 
		<< msg << endl;
	pvrInfo_oldMs = ms;
}
#endif

//////// only android //////
#ifdef __ANDROID__
#include <sys/stat.h>
#include <android/log.h>

using namespace std;

void pvrInfo(string msg) {

	auto result_code = mkdir("/storage/emulated/0/PVR/", 0777);
	auto *file = fopen("/storage/emulated/0/PVR/pvrlog.txt", "a");

	if (file) {
		fprintf(file, "PVR-JNI-I : %s\n", msg.c_str());
		fclose(file);
	}
}

void pvrdebug(string msg) {
	__android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "%s", msg.c_str());

	auto result_code = mkdir("/storage/emulated/0/PVR/", 0777);
	auto *file = fopen("/storage/emulated/0/PVR/pvrDebuglog.txt", "a");

	if (file) {
		fprintf(file, "PVR-JNI-D : %s\n", msg.c_str());
		fclose(file);
	}
}

#endif
