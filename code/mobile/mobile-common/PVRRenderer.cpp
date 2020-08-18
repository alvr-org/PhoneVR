#include "PVRRenderer.h"

#include <unistd.h>
#include "Geometry"

#include "Utils/RenderUtils.h"
#include "PVRSockets.h"

using namespace std;
using namespace gvr;
using namespace Eigen;

// globals

namespace PVR {
    unique_ptr<GvrApi> gvrApi;
}

using namespace PVR;

namespace {

    bool reproj = false;
    bool debugMode = false;
    float offFov;

    gvr_context *gvrCtx; // only for ios
    unique_ptr<SwapChain> swapChain;
    unique_ptr<BufferViewportList> vps;

    Matrix4f rotInv = Matrix4f::Identity();
    unique_ptr<Renderer> videoRdr[2];


    Matrix4f gvrToEigenMat(Mat4f gvrMat) {
        Matrix4f eMat;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                eMat(i, j) = gvrMat.m[i][j];
        return eMat;
    }

    const float deg2rad = (float)M_PI / 180;
    const float nearP = 0.1;
    const float farP = 10;

    Matrix4f Perspective(const Rectf &fov) {
        Matrix4f res = Matrix4f::Zero();
        float left = -tan(fov.left * deg2rad) * nearP;
        float top = tan(fov.top * deg2rad) * nearP;
        float right = tan(fov.right * deg2rad) * nearP;
        float bottom = -tan(fov.bottom * deg2rad) * nearP;

        res(0, 0) = (2 * nearP) / (right - left);
        res(0, 2) = (right + left) / (right - left);
        res(1, 1) = (2 * nearP) / (top - bottom);
        res(1, 2) = (top + bottom) / (top - bottom);
        res(2, 2) = (nearP + farP) / (nearP - farP);
        res(2, 3) = (2 * nearP * farP) / (nearP - farP);
        res(3, 2) = -1;

        return res;
    }

    struct EyeData {
        int vpLeft, vpBottom, vpWidth, vpHeight;
        Matrix4f quadModel, proj;
    } eyes[2];

    vector<float> leftQuad;

    void InitGVRRendering(){
        if (gvrApi) {
            gvrApi->RefreshViewerProfile();

            vector<BufferSpec> specs;
            specs.push_back(gvrApi->CreateBufferSpec());
            int rdrWidth = specs[0].GetSize().width;
            int rdrHeight = specs[0].GetSize().height;
            swapChain.reset(new SwapChain(gvrApi->CreateSwapChain(specs)));
            vps.reset(new BufferViewportList(gvrApi->CreateEmptyBufferViewportList()));
            vps->SetToRecommendedBufferViewports();

            for (size_t i = 0; i < 2; ++i) {
                EyeData e;
                auto bufVp = gvrApi->CreateBufferViewport();
                vps->GetBufferViewport(i, &bufVp);

                auto rect = bufVp.GetSourceUv();
                e.vpLeft = int(rect.left * rdrWidth);
                e.vpBottom = int(rect.bottom * rdrHeight);
                e.vpWidth = int((rect.right - rect.left) * rdrWidth);
                e.vpHeight = int((rect.top - rect.bottom) * rdrHeight);

                auto fov = bufVp.GetSourceFov();
                e.proj = Perspective(fov);

                Rectf quad;
                quad.left = -tan((fov.left + offFov) * deg2rad);
                quad.top = tan((fov.top + offFov * 2 / 3) * deg2rad); //augment less vertical fov
                quad.right = tan((fov.right + offFov) *  deg2rad);
                quad.bottom = -tan((fov.bottom + offFov * 2 / 3) * deg2rad);
                if (i == GVR_LEFT_EYE) {
                    leftQuad = {quad.left, quad.top, quad.right, quad.bottom};
                }
                e.quadModel = Affine3f(Translation3f((quad.right + quad.left) / 2.f, (quad.top + quad.bottom) / 2.f, -1.f)).matrix() *
                               Affine3f(Scaling((quad.right - quad.left) / 2.f, (quad.top - quad.bottom) / 2.f, 1.f)).matrix();

                eyes[i] = e;
            }
        }
    }

    // Matrix4f eyeMat = gvrToEigenMat(gvrApi->GetEyeFromHeadMatrix(eye)) * headMat;
    //lastRotMatInv = headMat.inverse();
}

unsigned int PVRInitSystem(int maxW, int maxH, float offFov, bool reproj, bool debug) {
    pvrState = PVR_STATE_INITIALIZATION;
    gvrApi->InitializeGl();
    //auto fjdkl = gvrApi->IsFeatureSupported(GVR_FEATURE_MULTIVIEW);

    ::offFov = offFov;
    ::reproj = reproj;
    debugMode = debug;

    InitGVRRendering();

    //block until receive nal headers. todo: optimize timing
    SendAdditionalData({(uint16_t)maxW, (uint16_t)maxH}, leftQuad,
                       gvrApi->GetEyeFromHeadMatrix(GVR_LEFT_EYE).m[0][3] * 2); // extracting IPD

    auto videoTex = genTexture(true);
    videoRdr[0].reset(new Renderer({{videoTex, true}}, FS_PT, 0.0f, 0.5f));
    videoRdr[1].reset(new Renderer({{videoTex, true}}, FS_PT, 0.5f, 1.0f));

    gvrApi->ResumeTracking();

    return videoTex;
}

void PVRRender(int64_t pts) {
    PVR_DB("Rendering " + to_string(pts));
    if (pvrState == PVR_STATE_RUNNING) {

        static Clk::time_point oldtime = Clk::now();

        if (pts > 0) {
            vector<float> v = DequeueQuatAtPts(pts);
            if (v.size() == 4)
                rotInv.block(0, 0, 3, 3) = Matrix3f(Quaternionf(v[0], v[1], v[2], v[3])); //todo: simplify
                //rotInv = rotMat;//.inverse(); ?
        }



        //vps->SetToRecommendedBufferViewports();
        Frame frame = swapChain->AcquireFrame();

        frame.BindBuffer(0);
        if (!debugMode)
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // black background
        else
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // red background
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        ClockTimePoint tgt_time = GvrApi::GetTimePointNow();
        tgt_time.monotonic_system_time_nanos += 20000000; // 0.020 s
        Mat4f gvrHeadMat = gvrApi->GetHeadSpaceFromStartSpaceRotation(tgt_time);
        Matrix4f deltaRot = gvrToEigenMat(gvrHeadMat) * rotInv;

        for (int i = 0; i < 2; ++i) {
            auto e = eyes[i];
            glViewport(e.vpLeft, e.vpBottom, e.vpWidth, e.vpHeight);
            glScissor(e.vpLeft, e.vpBottom, e.vpWidth, e.vpHeight);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Matrix4f mvp;
            if (reproj)
                mvp = e.proj * deltaRot * e.quadModel; //order sensitive!!
            else
                mvp = e.proj * e.quadModel;

            videoRdr[i]->render(mvp);
        }

        frame.Unbind();
        frame.Submit(*vps, gvrHeadMat);

        fpsRenderer = (1000000000.0 / (Clk::now() - oldtime).count());
        oldtime = Clk::now();
    }
}

void PVRTrigger() { } //TODO: register press

void PVRPause() {
    if (pvrState == PVR_STATE_RUNNING && gvrApi) {
        gvrApi->PauseTracking();
        pvrState = PVR_STATE_PAUSED;
    }
}

void PVRResume() {
    if (pvrState == PVR_STATE_PAUSED && gvrApi){
        InitGVRRendering();
        gvrApi->ResumeTracking();
        pvrState = PVR_STATE_RUNNING;
    }
}

void PVRCreateGVR(gvr_context *ctx) {
    gvrApi = GvrApi::WrapNonOwned(ctx);
}

void PVRDestroyGVR() {
    gvr_destroy(&gvrCtx);
}


#ifdef __APPLE__

void PVRCreateGVRAndContext() {
    gvrCtx = gvr_create();
    PVRCreateGVR(gvrCtx);
}
#endif
