
#include "PVRDesktopDupl.h"
#include "PVRServerGlobals.h"
#include <dxgi1_2.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "shaders\VertexShader.h"
#include "shaders\PixelShader.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using namespace std;

namespace {
	int outID;
	uint8_t *buffer;

	ID3D11Device *dxDev;
	ID3D11DeviceContext *dxDevCtx;
	IDXGIOutputDuplication *outDupl;

	ID3D11Texture2D *tgt;
	ID3D11Resource *tgtRes;

	vector<uint64_t[3]> texHdlSets;
	//ID3D11Texture2D *localTexs[3];

	void debugHr(HRESULT hr) {
		LPWSTR output;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&output, 0, NULL);
		wstring wstr = output;
		PVR_DEBUG(string(wstr.begin(), wstr.end()).c_str());
	}

#define RELEASE(obj) if(obj) { obj->Release(); obj = 0; }
#define OK_OR_EXEC(call, func) if(FAILED(call)) func();
#define OK_OR_DEBUG(call) { HRESULT hr = call; OK_OR_EXEC(hr, [hr] { PVR_DEBUG("####### d3d error: ######"); debugHr(hr); }) }
#define QUERY(from, to) from->QueryInterface(__uuidof(to), reinterpret_cast<void**>(&to))
#define PARENT(child, parent) child->GetParent(__uuidof(parent), reinterpret_cast<void**>(&parent))
#define GET_RES(hdl, res) dxDev->OpenSharedResource(hdl, __uuidof(res), reinterpret_cast<void**>(&res));
#define OK_OR_REINIT(call) OK_OR_EXEC(call, [](){ ReleaseDupl(); InitDupl(outID); })
}


void InitDupl(int id) {
	outID = id;

	auto fl = D3D_FEATURE_LEVEL_11_0;
	OK_OR_DEBUG(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dxDev, nullptr, &dxDevCtx));

	////get bounds and output duplication
	//IDXGIDevice* dxgiDev;
	//QUERY(dxDev, dxgiDev);
	//IDXGIAdapter* adapt;
	//PARENT(dxgiDev, adapt);
	//RELEASE(dxgiDev);
	//IDXGIOutput* dxgiOut;
	//OK_OR_DEBUG(adapt->EnumOutputs(outID, &dxgiOut));
	//RELEASE(adapt);
	//IDXGIOutput1* dxgiOut1;
	//QUERY(dxgiOut, dxgiOut1);
	//OK_OR_REINIT(dxgiOut1->DuplicateOutput(dxDev, &outDupl));
	//RELEASE(dxgiOut1);
	//DXGI_OUTPUT_DESC deskDesc;
	//dxgiOut->GetDesc(&deskDesc);
	//RELEASE(dxgiOut);
	//pvrBounds = deskDesc.DesktopCoordinates;
	pvrBounds = { 0,0,1920,1080 };

	//init render target
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = pvrWidth;
	texDesc.Height = pvrHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;// | D3D11_BIND_SHADER_RESOURCE;
	OK_OR_DEBUG(dxDev->CreateTexture2D(&texDesc, nullptr, &tgt));


	ID3D11RenderTargetView *rtv;
	OK_OR_DEBUG(dxDev->CreateRenderTargetView(tgt, nullptr, &rtv));
	dxDevCtx->OMSetRenderTargets(1, &rtv, nullptr);
	RELEASE(rtv);

	D3D11_RASTERIZER_DESC rtrDesc = {};
	rtrDesc.FillMode = D3D11_FILL_SOLID;
	rtrDesc.CullMode = D3D11_CULL_NONE;
	ID3D11RasterizerState *rtrState;
	OK_OR_DEBUG(dxDev->CreateRasterizerState(&rtrDesc, &rtrState));
	dxDevCtx->RSSetState(rtrState);

	//init viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)pvrWidth;
	vp.Height = (float)pvrHeight;
	vp.MaxDepth = 1.0f;
	dxDevCtx->RSSetViewports(1, &vp);

	struct VertexType { XMFLOAT3 position; XMFLOAT2 texture; } vertices[6] =
	{ { { -1, -1, 0 },{ 0, 1 } },{ { -1, 1, 0 },{ 0, 0 } },{ { 1, 1, 0 },{ 1, 0 } },
	{ { -1, -1, 0 },{ 0, 1 } },{ { 1, 1, 0 },{ 1, 0 } },{ { 1, -1, 0 },{ 1, 1 } } };

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(vertices);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices;
	ID3D11Buffer *vbuf;
	OK_OR_DEBUG(dxDev->CreateBuffer(&vertexBufferDesc, &vertexData, &vbuf));
	unsigned int stride = sizeof(VertexType);
	unsigned int offset = 0;
	dxDevCtx->IASetVertexBuffers(0, 1, &vbuf, &stride, &offset);
	RELEASE(vbuf);
	dxDevCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11VertexShader *vs;
	OK_OR_DEBUG(dxDev->CreateVertexShader(g_VS, ARRAYSIZE(g_VS), nullptr, &vs));
	dxDevCtx->VSSetShader(vs, nullptr, 0);
	RELEASE(vs);
	ID3D11PixelShader *ps;
	OK_OR_DEBUG(dxDev->CreatePixelShader(g_PS, ARRAYSIZE(g_PS), nullptr, &ps));
	dxDevCtx->PSSetShader(ps, nullptr, 0);
	RELEASE(ps);

	D3D11_INPUT_ELEMENT_DESC layoutDesc[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	ID3D11InputLayout* layout;
	OK_OR_DEBUG(dxDev->CreateInputLayout(layoutDesc, 2, g_VS, ARRAYSIZE(g_VS), &layout));
	dxDevCtx->IASetInputLayout(layout);
	RELEASE(layout);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;// D3D11_FILTER_MIN_MAG_MIP_POINT; 
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	ID3D11SamplerState *sampState;
	OK_OR_DEBUG(dxDev->CreateSamplerState(&samplerDesc, &sampState));
	dxDevCtx->PSSetSamplers(0, 1, &sampState);
	RELEASE(sampState);

	//init CPU-accessible texture resource
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.BindFlags = 0;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ID3D11Texture2D *tex;
	OK_OR_DEBUG(dxDev->CreateTexture2D(&texDesc, nullptr, &tex));
	QUERY(tex, tgtRes);
	RELEASE(tex);

	buffer = new uint8_t[pvrWidth * pvrHeight * 4];
}

void ReleaseDupl() {
	delete[] buffer;
	buffer = nullptr;//avoid exception when crash-reinitting
	RELEASE(tgtRes);
	RELEASE(tgt);
	RELEASE(outDupl);
	RELEASE(dxDevCtx);
	RELEASE(dxDev);
}

void DesktopToFrame() {
	printTime();
	timeStart();

	DXGI_OUTDUPL_FRAME_INFO fi;
	IDXGIResource *dxgiRes;
	auto hr = outDupl->AcquireNextFrame(1000, &fi, &dxgiRes);
	if (FAILED(hr)) {
		outDupl->ReleaseFrame();
		if (hr != DXGI_ERROR_WAIT_TIMEOUT) {//if not just timeout, reinitialize
			OK_OR_REINIT(hr);
		}
		return;
	}

	ID3D11Resource *d3dRes;
	QUERY(dxgiRes, d3dRes);
	RELEASE(dxgiRes);

	ID3D11ShaderResourceView *srv;
	dxDev->CreateShaderResourceView(d3dRes, nullptr, &srv);
	dxDevCtx->PSSetShaderResources(0, 1, &srv);
	RELEASE(srv);

	dxDevCtx->Draw(6, 0);
	dxDevCtx->Flush();

	dxDevCtx->CopyResource(tgtRes, tgt);

	IDXGISurface1 *dxgiSurf;
	QUERY(tgtRes, dxgiSurf);
	DXGI_MAPPED_RECT map;
	dxgiSurf->Map(&map, DXGI_MAP_READ);
	memcpy(buffer, map.pBits, pvrWidth * pvrHeight * 4);//block-copy all the texture to reduce cpu-gpu calls

	auto f0 = vFrame->data[0], f1 = vFrame->data[1], f2 = vFrame->data[2];
	for (int y = 0; y < pvrHeight; y++)
	{
		int line = y * pvrWidth;
		bool evenLine = y % 2 == 0;
		for (int x = 0; x < pvrWidth; x++)
		{
			int coord = line + x;
			int coordBuf = coord * 4;
			f0[coord] = buffer[coordBuf];
			if (evenLine && x % 2 == 0)
			{
				int coordHalf = line / 4 + x / 2;
				f1[coordHalf] = buffer[coordBuf + 1];
				f2[coordHalf] = buffer[coordBuf + 2];
			}
		}
	}

	dxgiSurf->Unmap();
	RELEASE(dxgiSurf);

	outDupl->ReleaseFrame();

	RELEASE(d3dRes);

}

