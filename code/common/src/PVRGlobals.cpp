#include "PVRGlobals.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace std;
using namespace std::chrono;


PVR_STATE pvrState = PVR_STATE_IDLE;

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

namespace 
{
	int64_t pvrdebug_oldMs = 0;
	int64_t pvrInfo_oldMs = 0;

	const wstring logFileDebug = L"\\..\\..\\drivers\\pvr\\logs\\pvrDebuglog.txt";
	const wstring logFileInfo = L"\\..\\..\\drivers\\pvr\\logs\\pvrlog.txt";
}

std::wstring _GetExePath(void)
{
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

void pvrdebug(string msg) 
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	ofstream of(std::wstring(_GetExePath() + logFileDebug).c_str(), ios_base::app);
	auto ms = system_clock::now().time_since_epoch() / 1ms;
	string elapsed = (ms - pvrdebug_oldMs < 1000 ? to_string(ms - pvrdebug_oldMs) : "max");
	of << t.wMinute << ":" << setfill('0') << setw(2) << t.wSecond << " " << setfill('0') << setw(3) << elapsed << "  " << msg << endl;
	
	OutputDebugStringA( ("--[PhoneVR]--"+msg+"\n").c_str() );

	pvrdebug_oldMs = ms;	
}

void pvrdebugClear() 
{
	ofstream(std::wstring(_GetExePath() + logFileDebug).c_str(), ofstream::out | ofstream::trunc);
}

void pvrInfo(string msg)
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	ofstream of(std::wstring(_GetExePath() + logFileInfo).c_str(), ios_base::app);
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
extern char* ExtDirectory;

void pvrInfo(string msg) {

	auto *file = fopen(string(string(ExtDirectory).c_str() + string("/PVR/pvrlog.txt")).c_str(),
					   "a");

	std::time_t nowtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm *time = localtime(&nowtime);

	if (file) {
		fprintf(file, "%02d:%02d:%02d-%02d.%02d.%04d - PVR-JNI-I: %s\n", time->tm_hour, time->tm_min, time->tm_sec,
				time->tm_mday, 1+time->tm_mon, 1900+time->tm_year, msg.c_str());
		fclose(file);
	}
}

void pvrdebug(string msg) {
	__android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "%s", msg.c_str());

	auto *file = fopen(string(string(ExtDirectory) +"/PVR/pvrDebuglog.txt").c_str(), "a");

	std::time_t nowtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm *time = localtime(&nowtime);

	if (file) {
		fprintf(file, "%02d:%02d:%02d-%02d.%02d.%04d - PVR-JNI-D: %s\n", time->tm_hour, time->tm_min, time->tm_sec,
				time->tm_mday, 1+time->tm_mon, 1900+time->tm_year, msg.c_str());
		fclose(file);
        //__android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "Wrote in File: %s - %s",	string(ExtDirectory).c_str(), string(string(ExtDirectory) + string("/PVR/pvrlog.txt")).c_str());
	}
    else {
        __android_log_print(ANDROID_LOG_DEBUG, "PVR-JNI-D", "File Error %d (%s): %s - %s",
                            errno,
                            string(ExtDirectory).c_str(),
                            string(string(ExtDirectory) + string("/PVR/pvrlog.txt")).c_str(),
                            strerror(errno));
    }
}


#endif
