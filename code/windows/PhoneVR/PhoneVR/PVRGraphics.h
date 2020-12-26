#pragma once
#include <vector>
#include <d3d11.h>

#include <amp_graphics.h>
#include "PVRGlobals.h"
#include <optional>


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")


using namespace std;
using namespace this_thread;
using namespace concurrency;
using namespace concurrency::direct3d;
using namespace concurrency::graphics;
using namespace concurrency::graphics::direct3d;

#define RELEASE(obj) if(obj) { obj->Release(); obj = nullptr; }
#define OK_OR_EXEC(call, func) if(FAILED(call)) func()
#define OK_OR_DEBUG(call) { HRESULT hr = call; OK_OR_EXEC(hr, [hr] { PVR_DB_I("######### DirectX Error: #########"); debugHr(hr); }); }
#define QUERY(from, to) OK_OR_DEBUG(from->QueryInterface(__uuidof(to), reinterpret_cast<void**>(&to)))
//#define PARENT(child, parent) OK_OR_DEBUG(child->GetParent(__uuidof(parent), reinterpret_cast<void**>(&parent)))
//#define OK_OR_REINIT(call) OK_OR_EXEC(call, [] { PVRReleaseDX(); PVRInitDX(outID); })

// RGB to YUV constants
#define wr 0.299f
#define wb 0.114f
#define wg 0.587f // 1.0 - wr - wb;
#define ku 0.492f //0.436 / (1.0 - wb);
#define kv 0.877f //0.615 / (1.0 - wr);

void PVRInitDX();
void PVRUpdTexHdl(uint64_t texHdl, int whichBuffer);
void PVRStartGraphics(std::vector<std::vector<uint8_t *>> vvbuf, uint32_t width, uint32_t height);
void PVRStopGraphics();
void PVRReleaseDX();

extern float fpsRenderer;