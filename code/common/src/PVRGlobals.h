#pragma once

inline int mod3(int a) {
	int res = a % 3;
	return res >= 0 ? res : res + 3;
}

enum PVR_ERR {
	PVR_ERR_UNKNOWN = -1,
	PVR_ERR_NONE = 0,
	PVR_ERR_UDP_TIMEOUT,
	PVR_ERR_UDP_MANUAL_STOP,
	PVR_ERR_UDP_DESTROYED,
	PVR_ERR_INVALID_RESPONSE,
	PVR_ERR_INCOMPATIBLE_CLIENT,
	PVR_ERR_INCOMPATIBLE_SERVER,
	PVR_ERR_EXPIRED,
};

enum PVR_STATE {
	PVR_STATE_UNKNOWN = -1,
	PVR_STATE_IDLE = 0,
	PVR_STATE_INITIALIZATION,
	PVR_STATE_RUNNING,
	PVR_STATE_PAUSED,
	PVR_STATE_SHUTDOWN,
};

extern PVR_STATE pvrState;

#ifdef __cplusplus

#include <functional>
#include <chrono>
#include <thread>
#include <mutex>

#include "Utils/StrUtils.h"
#include "Utils/ThreadUtils.h"


//////// DEBUG //////////
void pvrdebug(std::string msg);

inline void pvrdebug(std::wstring msg) {
	pvrdebug(std::string(msg.begin(), msg.end()));
}

template<typename T>
void pvrdebug(T i) {
	pvrdebug(std::to_string(i));
}

//////// Info //////////
void pvrInfo(std::string msg);

inline void pvrInfo(std::wstring msg) {
	pvrInfo(std::string(msg.begin(), msg.end()));
}

template<typename T>
void pvrInfo(T i) {
	pvrInfo(std::to_string(i));
}

void pvrdebugClear();

// PVR_DB only to Debug Log file, PVR_DB_I both to Log file and Info file
#if defined _DEBUG
	#define PVR_DB(msg) pvrdebug(msg)
#else
	#define PVR_DB(msg)
#endif

#if defined _DEBUG
    #define PVR_DB_I(msg) {pvrdebug(msg);pvrInfo(msg);}
#else
	#define PVR_DB_I(msg) pvrInfo(msg);
#endif

#define PVR_DB_CLEAR() pvrdebugClear()

#define SAFE_DEL(obj) if (obj) { delete obj; obj = nullptr; }


// if a version of this program get hacked or cracked, it will eventually expire and quit on start
//bool PVRCheckIfExpired();

inline uint32_t vec2uint(uint8_t *v) {
	uint32_t i;
	memcpy(&i, v, 4);
	return i;
}

inline uint32_t vers2uint(uint8_t rel, uint8_t sub, uint16_t patch) {
	return rel << 24 | sub << 16 | patch;
}

#define PVR_SERVER_VERSION vers2uint(0, 6, 0)
#define PVR_CLIENT_VERSION vers2uint(0, 6, 0)

#endif //__cplusplus