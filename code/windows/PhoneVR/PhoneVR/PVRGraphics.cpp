#include "PVRGraphics.h"

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

namespace {
	ID3D11Device *dxDev;
	ID3D11DeviceContext *dxDevCtx;

	uint64_t curHdl = 0;
	int whichBuf;

	void debugHr(HRESULT hr) {
		LPWSTR output;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&output, 0, NULL);
		PVR_DB_I(wstring(output));
	}

	mutex texMtx;
	bool gRunning = false;
	thread *gThr;
}

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


void PVRInitDX() {
	auto fl = D3D_FEATURE_LEVEL_11_0;
	OK_OR_DEBUG(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dxDev, nullptr, &dxDevCtx));
}

void PVRUpdTexHdl(uint64_t texHdl, int whichBuf) {
	if (gRunning) {
		//wait to obtain TextureMutex lock = Wait for the Renderer to complete rendering of present frame
		texMtx.lock();
		::whichBuf = whichBuf;
		curHdl = texHdl;
		texMtx.unlock();
		while (curHdl != 0 && gRunning)
			sleep_for(500us);
	}
}

void PVRStartGraphics(vector<vector<uint8_t *>> vvbuf, uint32_t inpWidth, uint32_t inpHeight) {
	gRunning = true;
	gThr = new thread([=] {
		vector<vector<array_view<uint, 2>>> yuvBufViews;   // output

		for (auto vbuf : vvbuf) // divide width by 4: store 4 bytes in an uint
			yuvBufViews.push_back({ array_view<uint, 2>(inpHeight, inpWidth / 4, reinterpret_cast<uint *>(vbuf[0])),
								  array_view<uint, 2>(inpHeight / 2, inpWidth / 4 / 2, reinterpret_cast<uint *>(vbuf[1])),
								  array_view<uint, 2>(inpHeight / 2, inpWidth / 4 / 2, reinterpret_cast<uint *>(vbuf[2])) });

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = inpWidth;
		texDesc.Height = inpHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		ID3D11Texture2D *stagingTex;
		OK_OR_DEBUG(dxDev->CreateTexture2D(&texDesc, nullptr, &stagingTex));
		auto accView = create_accelerator_view(dxDev);
		auto ampTex = make_texture<unorm4, 2>(accView, stagingTex);

		const int texWidth = ampTex.extent[1], texHeight = ampTex.extent[0];

		while (curHdl == 0 && gRunning)
			sleep_for(500us);
		while (gRunning) {
						
			// Lock texture Handle until this frame is completely rendered
			texMtx.lock();

			if (!dxDev) {
				texMtx.unlock();
				continue;
			}

			static Clk::time_point oldtime;
			oldtime = Clk::now();

			//PVR_DB(curHdl);
			ID3D11Texture2D *inpTex;
			OK_OR_DEBUG(dxDev->OpenSharedResource((HANDLE)curHdl, __uuidof(ID3D11Texture2D), (void **)&inpTex));
			IDXGIKeyedMutex *dxMtx;
			QUERY(inpTex, dxMtx);
			if (SUCCEEDED(dxMtx->AcquireSync(0, 500))) {
				dxDevCtx->CopyResource(stagingTex, inpTex); //texAmp is automatically updated
				dxMtx->ReleaseSync(0);
			}
			RELEASE(dxMtx);
			RELEASE(inpTex);

			auto outY = yuvBufViews[whichBuf][0];
			auto outU = yuvBufViews[whichBuf][1];
			auto outV = yuvBufViews[whichBuf][2];

			// gpu kernel:
			concurrency::parallel_for_each(concurrency::extent<2>(inpHeight / 2, inpWidth / 8), [=, &ampTex](index<2> idx) restrict(amp) {
				//get texture coordinates
				int ty = idx[0] * 2, tx = idx[1] * 8;

				uint yuv[2][8][3];
				for (int y = 0; y < 2; y++) {
					for (int x = 0; x < 8; x++) {
						// retrieve srgb pixel
						unorm3 srgb = ampTex[index<2>(ty + y, tx + x)].rgb;
						//// convert to yuv
						float Y = wr * srgb.r + wg * srgb.g + wb * srgb.b;
						yuv[y][x][0] = uint(Y * 256.f);
						yuv[y][x][1] = uint(ku * (srgb.b - Y) * 256.f + 128.f);
						yuv[y][x][2] = uint(kv * (srgb.r - Y) * 256.f + 128.f);

						for (int i = 0; i < 3; i++)
							yuv[y][x][i] = min(yuv[y][x][i], 255);
					}
				}

				// pack Y channel: save 4 quadruplets in 4 uint
				// { | | | }{ | | | }
				// { | | | }{ | | | }
				for (int y = 0; y < 2; y++)
					for (int x = 0; x < 2; x++)
						outY[idx * 2 + index<2>(y, x)] = yuv[y][x * 4][0] | yuv[y][x * 4 + 1][0] << 8 | yuv[y][x * 4 + 2][0] << 16 | yuv[y][x * 4 + 3][0] << 24;

				// pack U & V channel: get quadruplet of average 2x2 squares
				// / | \/ | \/ | \/ | \
				// \ | /\ | /\ | /\ | /
				uint u[] = { 0, 0, 0, 0 }, v[] = { 0, 0, 0, 0 };
				for (int c = 0; c < 4; c++) {
					for (int y = 0; y < 2; y++) {
						for (int x = 0; x < 2; x++) {
							u[c] += yuv[y][c * 2 + x][1];
							v[c] += yuv[y][c * 2 + x][2];
						}
					}
					u[c] /= 4;
					v[c] /= 4;
				}
				//save quadruplet in uint
				outU[idx] = yuv[0][0][1] | yuv[0][2][1] << 8 | yuv[0][4][1] << 16 | yuv[0][6][1] << 24;
				outV[idx] = yuv[0][0][2] | yuv[0][2][2] << 8 | yuv[0][4][2] << 16 | yuv[0][6][2] << 24;
			});
			outY.synchronize();
			outU.synchronize();
			outV.synchronize();

			curHdl = 0;
			texMtx.unlock();

			fpsRenderer = (1000000000.0 / (Clk::now() - oldtime).count());
			oldtime = Clk::now();

			// Wait until we receive a new SharedTextHandle Updated from 
			// PVRGraphics::PVRUpdTexHdl() <- PVRSockets::PVRProcessFrame() <- OpenVR::Present()
			while (curHdl == 0 && gRunning)
				sleep_for(500us);
			
			//PVR_DB("[PVRGraphics th] Rendering @ FPS: " + to_string(fpsRenderer) );
		}
		RELEASE(stagingTex);
	});
}

void PVRStopGraphics() {
	gRunning = false;
	EndThread(gThr);
}

void PVRReleaseDX() {
	texMtx.lock();
	RELEASE(dxDevCtx);
	RELEASE(dxDev);
	texMtx.unlock();
}