#include "PVRGraphics.h"

#include <d3d11.h>
#include <amp_graphics.h>
#include "PVRGlobals.h"


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
	/*
		uint32_t eyeWidth, eyeHeight, totWidth;*/

		//accelerator_view *accView;

	uint64_t oldHdl = 0, curHdl = 0;
	int whichBuf;
	//vector<texture<unorm4, 2>> currAmpTexs;                               // curr input
	//map<uint64_t, tuple<vector<uint64_t>, ID3D11Texture2D *>> texMap;     // all inputs
	ID3D11Texture2D *currDxTex;

	vector<vector<array_view<uint, 2>>> yuvBufViews;                      // output

	void debugHr(HRESULT hr) {
		LPWSTR output;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&output, 0, NULL);
		PVR_DB(wstring(output));
	}

	mutex texMtx;
	bool gRunning = false;
	thread *gThr;
}

#define RELEASE(obj) if(obj) { obj->Release(); obj = nullptr; }
#define OK_OR_EXEC(call, func) if(FAILED(call)) func();
#define OK_OR_DEBUG(call) { HRESULT hr = call; OK_OR_EXEC(hr, [hr] { PVR_DB("####### d3d error: ######"); debugHr(hr); }) }
#define QUERY(from, to) OK_OR_DEBUG(from->QueryInterface(__uuidof(to), reinterpret_cast<void**>(&to)))
//#define PARENT(child, parent) OK_OR_DEBUG(child->GetParent(__uuidof(parent), reinterpret_cast<void**>(&parent)))
#define GET_RES(dev, hdl, res) OK_OR_DEBUG(dev->OpenSharedResource(hdl, __uuidof(res), reinterpret_cast<void**>(&res)))
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

void PVRCreateSharedTexSet(uint32_t fmt, uint64_t texHdls[3], uint32_t width, uint32_t height) {
	texMtx.lock();
	PVR_DB("Format: " + to_string(fmt));
	PVR_DB("Width: " + to_string(width));
	PVR_DB("Height: " + to_string(height));

	ID3D11Texture2D *texs[3]; // array of pointers!

	uint64_t hdls[3];

	for (size_t i = 0; i < 3; i++) {
		if (fmt != (DXGI_FORMAT)29) {
			PVR_DB("wrong dxgi format");
			exit(1);
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = (DXGI_FORMAT)fmt;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		OK_OR_DEBUG(dxDev->CreateTexture2D(&texDesc, nullptr, &texs[i]));

		IDXGIResource *texRes;
		QUERY(texs[i], texRes);

		HANDLE hdl;
		OK_OR_DEBUG(texRes->GetSharedHandle(&hdl));
		texHdls[i] = reinterpret_cast<uint64_t>(hdl);
		hdls[i] = texHdls[i];
		RELEASE(texRes);
	}

	for (size_t i = 0; i < 3; i++) {
		texMap.insert({ texHdls[i], {{hdls[0], hdls[1], hdls[2]}, texs[i]} });
	}
	texMtx.unlock();
}

void PVRUpdTexHdls(uint64_t texHdl, int whichBuffer) {
	if (gRunning) {
		texMtx.lock();
		whichBuf = whichBuffer;
		curHdl = texHdl;
		texMtx.unlock();
		while (curHdl != 0 && gRunning)
			sleep_for(500us);
	}
}

void PVRStartGraphics(vector<vector<uint8_t *>> vvbuf, uint32_t eyeWidth, uint32_t eyeHeight) {
	gRunning = true;
	gThr = new thread([=] {
		for (auto vbuf : vvbuf)
			yuvBufViews.push_back({ array_view<uint, 2>(eyeHeight, (eyeWidth * 2) / 4, reinterpret_cast<uint *>(vbuf[0])),
								  array_view<uint, 2>(eyeHeight / 2, (eyeWidth * 2) / 8, reinterpret_cast<uint *>(vbuf[1])),
								  array_view<uint, 2>(eyeHeight / 2, (eyeWidth * 2) / 8, reinterpret_cast<uint *>(vbuf[2])) });

		while (curHdl == 0 && gRunning)
			sleep_for(500us);
		while (gRunning) {
			texMtx.lock();

			if (curHdl != oldHdl) {
				currAmpTexs.clear();
				if (!dxDev) {
					texMtx.unlock();
					return;
				}

				auto it0 = texMap.find(curHdl);
				if (it0 == texMap.end() || it1 == texMap.end()) {// textures missing, nothing to do
					texMtx.unlock();
					return;
				}

				PVR_DB("Creating texture wrappers");

				auto accView = create_accelerator_view(dxDev);
				currAmpTexs.push_back(make_texture<unorm4, 2>(accView, get<1>(texMap.at(curHdl))));
				currAmpTexs.push_back(make_texture<unorm4, 2>(accView, get<1>(texMap.at(curHdl1))));

				oldHdl = curHdl;
			}

			auto outY = yuvBufViews[whichBuf][0];
			auto outU = yuvBufViews[whichBuf][1];
			auto outV = yuvBufViews[whichBuf][2];

			auto tex0 = currAmpTexs[0];
			auto tex1 = currAmpTexs[1];
			const int /*width = eyeWidth, height = eyeHeight, */texWidth = tex0.extent[1], texHeight = tex0.extent[0];


			// gpu kernel:
			concurrency::parallel_for_each(concurrency::extent<2>(eyeHeight / 2, eyeWidth / 4), [=, &tex0, &tex1](index<2> idx) restrict(amp) {

				//get texture coordinates
				int ty = idx[0] * 2, tx = (idx[1] * 8) % eyeWidth;
				bool whichTex = (idx[1] * 8 < eyeWidth);

				uint yuv[2][8][3];
				for (int y = 0; y < 2; y++) {
					for (int x = 0; x < 8; x++) {
						// retrieve srgb pixel
						unorm3 srgb = (whichTex ? tex0 : tex1)[index<2>((ty + y) * texHeight / eyeHeight, (tx + x) * texWidth / eyeWidth)].rgb;

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
			while (curHdl == 0 && gRunning)
				sleep_for(500us);
		}
		currAmpTexs.clear(); // this is retaining d3d textures
		yuvBufViews.clear();
	});
}

void PVRStopGraphics() {
	gRunning = false;
	PVREndThr(gThr);
}

void PVRReleaseSharedTexSet(uint64_t hdl) {
	texMtx.lock();
	auto it = texMap.find(hdl);
	if (it != texMap.end()) {
		auto tmpHdls = get<0>(it->second); // after texMap element is erased, this become invalid
		uint64_t hdls[] = { tmpHdls[0], tmpHdls[1], tmpHdls[2] };
		for (auto h : hdls) {
			RELEASE(get<1>(texMap.at(h)));
			texMap.erase(h);
		}
	}
	texMtx.unlock();
}

void PVRReleaseAllSharedTexSets() {
	texMtx.lock();
	//while (texMap.size() > 0)
	//	PVRReleaseSharedTexSet(texMap.begin()->first);

	for (auto t : texMap)
		RELEASE(get<1>(t.second));
	texMap.clear();
	texMtx.unlock();
}

void PVRReleaseDX() {
	PVRReleaseAllSharedTexSets();
	texMtx.lock();
	RELEASE(dxDevCtx);
	RELEASE(dxDev);
	texMtx.unlock();
}