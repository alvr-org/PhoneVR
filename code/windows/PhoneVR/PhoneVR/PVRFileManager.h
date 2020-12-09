#pragma once
#include <fstream>
#include "nlohmann/json.hpp" //awesome lib!
#include <string>
#include <optional>

#include "PVRGlobals.h"


//global char* need to be double const
typedef const char *const ccc;

ccc ENABLE_KEY = "enable";
ccc GAME_FPS_KEY = "game_fps";
//ccc STREAM_FPS_KEY = "video_stream_fps";
//ccc LATENCY_KEY = "latency_correction";
//ccc WARP_KEY = "warp_correction";
//ccc BLUR_KEY = "blur_enable";
ccc VIDEO_PORT_KEY = "video_stream_port";
ccc POSE_PORT_KEY = "pose_stream_port";
ccc CONN_PORT_KEY = "pairing_port";

ccc ENCODER_SECT = "encoder";
ccc PRESET_KEY = "preset";
ccc TUNE_KEY = "tune";
ccc RC_METHOD_KEY = "rc_method";
ccc QP_KEY = "qp";
ccc QCOMP_KEY = "qcomp";
ccc KEYINT_MAX_KEY = "keyint_max";
ccc I_REFRESH_KEY = "intra_refresh";
ccc BITRATE_KEY = "bitrate";
ccc PROFILE_KEY = "profile";
ccc CONN_TIMEOUT = "connection_timeout";

namespace 
{
	const nlohmann::json defSets = 
	{
		{ENABLE_KEY, true},
		{GAME_FPS_KEY, 60},
		//{STREAM_FPS_KEY, 60},
		//{LATENCY_KEY, 0.070},
		//{WARP_KEY, 0.0},
		//{BLUR_KEY, true},
		{VIDEO_PORT_KEY, 15243},
		{POSE_PORT_KEY, 51423},
		{CONN_PORT_KEY, 33333},
		{CONN_TIMEOUT, 5},
		{ENCODER_SECT, {
			{PRESET_KEY, "ultrafast"},
			{TUNE_KEY, "zerolatency"},
			{RC_METHOD_KEY, 1},
			{QP_KEY, 20},
			{QCOMP_KEY, 0.0},
			{KEYINT_MAX_KEY, -1},
			{I_REFRESH_KEY, false},
			{BITRATE_KEY, -1},
			{PROFILE_KEY, "baseline"},
						}
		}
	};

	//const wchar_t *const setsFile = L"C:\\Program Files\\PhoneVR\\pvrsettings.json";
	const wchar_t *const setsFile = L"\\..\\..\\drivers\\PVRServer\\pvrsettings.json";
	
	nlohmann::json PVRGetSets() 
	{
		try 
		{
			return nlohmann::json::parse(std::ifstream( std::wstring(_GetExePath() + std::wstring(setsFile)).c_str() ));
		}
		catch (const std::exception& err)
		{
			PVR_DB_I( std::string("Error retrieving Setting: ") + err.what() );
			if (_wfopen( std::wstring(_GetExePath()+std::wstring(setsFile)).c_str() , L"r")) { PVR_DB_I("\t\tFile Exists !") }
			else { PVR_DB_I("\t\tFile does NOT Exist !"); }
			PVR_DB_I( std::wstring(L"\t\tSettingsFile @ ") + std::wstring(setsFile) );
			PVR_DB_I( std::wstring(L"\t\tCurrentDirectory @ ")+_GetExePath() );
		}
		return nlohmann::json();
	}

	void PVRSaveSets(nlohmann::json j) 
	{
		std::ofstream(setsFile) << j.dump(4);
	}
}

template <typename T>
void PVRSetProp(std::vector<std::string> propPath, T value) 
{
	auto j = PVRGetSets();
	auto *pj = &j;
	for (size_t i = 0; i < propPath.size(); i++)
		pj = &((*pj)[propPath[i]]);
	*pj = value;
	PVRSaveSets(j);
}

template<typename T>
T PVRProp(std::vector<std::string> propPath) 
{
	auto j = PVRGetSets();

	for (size_t i = 0; i < propPath.size(); i++) 
	{
		if (j.find(propPath[i]) != j.end()) 
		{
			j = j[propPath[i]];
			if (i == propPath.size() - 1)
				return j;
		}
		else
			break;
	}
	std::string fullPath;
	j = defSets;
	for (size_t i = 0; i < propPath.size(); i++) 
	{
		j = j[propPath[i]];
		fullPath += propPath[i] + (i < propPath.size() - 1 ? "." : "");
	}
	T res = j;
	PVR_DB_I("Using default setting value: " + fullPath + " = " + to_string(res));
	return res;
}


