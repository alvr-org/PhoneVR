/*references:
https://dsp.stackexchange.com/a/2237
https://stackoverflow.com/a/9592454
*/


/*
 * orientation-> poll from gvr (with some usecs ahead)
 * rotation rate -> do nothing, steamvr is fine
 * rotational acc -> do nothing
 * position -> old_pos + old_vel * delta t, send to steamvr
 * lin velocity -> old_vel + old_acc * delta t, send to steamvr
 * lin_acc -> orientation * cur_acc, get from ndk api, (light filtering), do not send to steamvr
 */


#include <jni.h>
#include <android/sensor.h>
#include <android/looper.h>

#include <string>
#include <thread>
#include <unistd.h>
#include "Watch.h"
#include "StrUtils.h"
#include "RenderUtils.h"

using namespace std;
using namespace Eigen;

#define FUNC(type, func) extern "C" JNIEXPORT type JNICALL Java_com_rz_opencvtest_MainActivity_##func
#define SUB(func) FUNC(void, func)

namespace{
    //consts
    const int camW = 1280, camH = 720;
    //const int camW = 640, camH = 360;
    const float dw = 1.f / camW, dh = 1.f / camH;
    const int mbW = 16, mbH = 15; //macroblock
    const int bufW = camW / mbW, bufH = camH / mbH;
    const int bufSz = bufW * bufH * 8;
    uint16_t texBuf[bufSz];


    float accData[3], orientData[4];


    vector<vector<int>> feats;


    GLuint camTex;
    Renderer *camRdr, *derivRdr, *xGaussRdr, *yGaussRdr, *harrisRdr, *xNmsRdr, *yNmsRdr, *nmsRdr, *compRdr, *harrisVisualRdr;
    PBO *pbo;


    //Matrix4f rotMat = Eigen::Affine3f(AngleAxisf(-(float)M_PI_2,Vector3f::UnitZ())).matrix(); // Affine3f is ambiguous??
}


FUNC(jint, InitGL)(JNIEnv *env, jobject){

    //tip: use inout to access previous render target data


    camTex = genTexture(true);
    camRdr = new Renderer({{camTex, true}});


    derivRdr = new Renderer(camW, camH, {{camTex, true}}, str_fmt(crypt(R"glsl(
        const float dw = %f, dh = %f;

        float gray(float xoff, float yoff){
            vec3 p = texture(tex0, vec2(coord.x + xoff, coord.y + yoff)).rgb;
            return p.r * .2125 + p.g * .7154 + p.b * .0721;
        }
        void main() {
            float pLeft = gray(-dw, 0.), pTop = gray(0., -dh), pRight = gray(dw, 0.), pDown = gray(0., dh);
            float Ix = (pRight - pLeft), Iy = (pDown - pTop);
            color = vec4(Ix * Ix, Iy * Iy, Ix * Iy / 2. + .5, 1);
        }
    )glsl"), dw, dh), GL_RGBA16_EXT);
    // output: {IxIx * 4, IyIy * 4, IxIy * 2 + .5, ignored}

    string gaussFS = crypt(R"glsl(
        const float dw = %f, dh = %f;

        vec3 getPix(float xpoff, float ypoff) {
            return texture(tex0, vec2(coord.x + xpoff * dw, coord.y + ypoff * dh)).xyz;
        }
        void main() {
            // variance = 1.0
            color = vec4(getPix(0., 0.) * .38774 +
                         (getPix(-1., -1.) + getPix(1., 1.)) * .24477 +
                         (getPix(-2., -2.) + getPix(2., 2.)) * .06136, 1);
        }
    )glsl");
    xGaussRdr = new Renderer(camW, camH, {{derivRdr->target(), false}}, str_fmt(gaussFS, dw, 0.f), GL_RGBA16_EXT);
    yGaussRdr = new Renderer(camW, camH, {{xGaussRdr->target(), false}}, str_fmt(gaussFS, 0.f, dh), GL_RGBA16_EXT);

    //Harris corner detector
    harrisRdr = new Renderer(camW, camH, {{yGaussRdr->target(), false}}, str_fmt(crypt(R"glsl(

        void main() {
            vec3 d = texture(tex0, coord).xyz;
            float sIxIx = d.x, sIyIy = d.y, sIxIy = (d.z - .5) * 2.;

            // product of eigenvalues = det + (trace^2)/2
            // det -> cornerness, trace^2 -> edgeness
            //float h = (sIxIx * sIyIy - sIxIy * sIxIy) - 0.01 *(sIxIx + sIyIy) * (sIxIx + sIyIy) / 2.;

            float h = (1. - (sIxIx * sIyIy - sIxIy * sIxIy)) / max((sIxIx + sIyIy), .0001)

            //Alison Noble, "Descriptions of Image Surfaces", 1989
            //float h = (sIxIx * sIyIy - sIxIy * sIxIy) / (sIxIx + sIyIy);

            color = vec4(h, 0, 0, 0);
        }
    )glsl")), GL_R16_EXT);
    //output: {harris value * 4}

    //Don't break the wavefront! No dynamic conditional expressions allowed
    //For each macroblock search the maximum harris value within an area 2*mbW x 2*mbH
    //if max value is not inside of macroblock, set max value to 0
    xNmsRdr = new Renderer(bufW, camH, {{harrisRdr->target(), false}}, str_fmt(crypt(R"glsl(
        const float dw = %f;
        const float win = %f;
        const float win_2 = win / 2.;

        void main() {
            float xoff = 0.;
            float h = 0.;
            for (float i = -win_2; i < win + win_2; i++) {
                // coord points to center of window/macroblock -> allineate to origin
                float new_h = texture(tex0, vec2(coord.x + (i + .5 - win_2) * dw, coord.y)).r;
                float enable = step(h, new_h);
                xoff += (i - xoff) * enable;      //conditional overwrite without if!!
                h += (new_h - h) * enable;
            }
            color = vec4(h * step(.0, xoff) * (1. - step(win, xoff)), (xoff + win_2) / 65536., 0, 0);
        }
    )glsl"), dw, (float)mbW), GL_RG16_EXT, GL_NEAREST);

    yNmsRdr = new Renderer(bufW, bufH, {{xNmsRdr->target(), false}}, str_fmt(crypt(R"glsl(
        const float dh = %f;
        const float win = %f;
        const float win_2 = win / 2.;

        void main() {
            float xoff = 0., yoff = 0.;
            float h = 0.;
            for (float i = -win_2; i < win + win_2; i++) {
                vec2 p = texture(tex0, vec2(coord.x, coord.y + (i + .5 - win_2) * dh)).rg;
                float new_h = p.r;
                float enable = step(h, new_h);
                xoff += (p.g - xoff) * enable;
                yoff += (i - yoff) * enable;
                h += (new_h - h) * enable;
            }
            color = vec4(h * step(.0, yoff) * (1. - step(win, yoff)), xoff, (yoff + win_2) / 65536., 1);
        }
    )glsl"), dh, (float)mbH), GL_RGBA16_EXT, GL_NEAREST);
    //output: {harris value, (x offset + win / 2) / 2^16, (y offset + win / 2) / 2^16, ignored}

    //todo: pass to pyramid optical flow
    //final output: {harris value, xy position in macroblock (8+8 bits), x flow offset, y flow offset}

    // Draw cross aim over filtered harris features
    // using mag GL_NEAREST in the input texture, the values do not get distorted if not sampling exactly in the center of pixels
    compRdr = new Renderer({{camTex, true}, {yNmsRdr->target(), false}}, str_fmt(R"glsl(
        const float width = %f, height = %f;
        const float winW = %f, winH = %f;
        const float winW_2 = winW / 2., winH_2 = winH / 2.;

        void main() {
            vec3 p = texture(tex1, coord).rgb;
            float h = p.r, xoff = p.g * 65536. - winW_2, yoff = p.b * 65536. - winH_2;

            // enable if: h is big enough, current coordinate matches with x or y offset
            float enable = step(.25, h * 64.) * max(step(abs(mod(coord.x * width, winW) - xoff), 1.),
                                                    step(abs(mod(coord.y * height, winH) - yoff), 1.));
            color = vec4(texture(tex0, coord).rgb * (1. - enable) + vec3(h * 128. * enable), 1);
        }
    )glsl", (float)camW, (float)camH, (float)mbW, (float)mbH));

    harrisVisualRdr = new Renderer({{harrisRdr->target(), false}}, str_fmt(R"glsl(
        void main() {
            float h = texture(tex0, coord).r * 128.;
            color = vec4(h, h, h, 1);
        }
    )glsl"));

//    pbo = new PBO(texBuf, bufW, bufH, bufSz);

    return camTex;
}

SUB(Draw)(JNIEnv *env, jobject, int w, int h, int which) {

    if (which > 0) {
        derivRdr->render();
        xGaussRdr->render();
        yGaussRdr->render();
        harrisRdr->render();
        xNmsRdr->render();
        yNmsRdr->render();
    }

//    pbo->download();

//    for (int y = 0; y < bufH; ++y) {
//        for (int x = 0; x < bufW; ++x) {
//            int idx = (y * camW + x) * 4;
//            int xoff = texBuf[idx + 2] - 5, yoff = texBuf[idx + 3] - 5;
//            if (xoff >= 0 && xoff < 10 && yoff >= 0 && yoff < 10) {
//                feats.push_back({(int)texBuf[idx] * 256 + texBuf[idx + 1], xoff, yoff});
//            }
//        }
//    }
//    feats.clear();


//    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
//    glEnable(GL_DEPTH_TEST);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupRTS(w, h);

    if (which == 0)
        camRdr->render();
    else if (which == 1)
        compRdr->render();
    else if (which == 2)
        harrisVisualRdr->render();
}

SUB(SetSensData)(JNIEnv *env, jobject, jint which, jfloatArray jdata, jlong jts){
    auto *accArr = env->GetFloatArrayElements(jdata, nullptr);
    if (which == 1){
        memcpy(orientData, accArr, 4 * 4);
    }
    env->ReleaseFloatArrayElements(jdata, accArr, JNI_ABORT);
}


//    string grayFS = crypt(R"glsl(
//        void main() {
//            vec3 p = texture(tex0, coord).rgb;
//            color = vec4((p.r + p.g + p.b) / 3.0, 0, 0, 1);
//        }
//    )glsl");

/*string nmsFS = str_fmt(crypt(R"glsl(
        const highp float dw = %f;
        const highp float dh = %f;


        highp float build(highp float xoff, highp float yoff) {
            highp vec4 p = texture(tex0, vec2(coord.x + xoff, coord.y + yoff));
            return p.x + p.y / 256.;
            //return p.y / 256.;
        }
        void main() {
            highp float h = build(0., 0.);
            //highp vec4 p2 = texture(tex0, coord);
            //highp float h2 =  p2.x + p2.y / 256.;


            //branching without if: step: a <= b ? 1 : 0;
            // mult == 1 if 8 adjacent pixs are strictly < than current pix
            highp float mult = (1. - step(h, build(0., -dh))) *
                               (1. - step(h, build(-dw, -dh))) *
                               (1. - step(h, build(-dw, 0.))) *
                               (1. - step(h, build(-dw, dh))) *
                               (1. - step(h, build(0., dh))) *
                               (1. - step(h, build(dw, dh))) *
                               (1. - step(h, build(dw, 0.))) *
                               (1. - step(h, build(dw, -dh)));
            mult = 1.0;
            float p = h * mult * 1.0;
            //highp float p = h * 50.0;/* step(textureLod(tex0, coord, %f).r * 10.0, h)
            color = vec4(p, p, p, 1);
        }
    )glsl"), dw, dh);
string xEvVarFS = R"glsl(
        const highp float d = %f;

        highp float build(int xoff) {
            highp vec4 p = texture(tex0, vec2(coord.x + (float)xoff * d, coord.y));
            return p.x + p.y / 256.;
        }
        void main() {
            highp float arr[5];
            highp float expVal = 0;
            for (int i = -2; i <= 2; i++) {
                highp float h = build(i);
                arr[i + 2] = h;
        }
    )glsl";
*/

//////////////////TUTTO SBAGLIATO/////////////////////
/*   string fastFS = str_fmt(crypt(R"glsl(
       const float dx = %f;
       const float dy = %f
       const float lod = %f;
       const int win = %i;
   )glsl"), 1.0 / camW, 1.0 / camH, (float)fastLod, winSz);
   fastFS += crypt(R"glsl(
       //http://docs.opencv.org/3.0-beta/doc/py_tutorials/py_feature2d/py_fast/py_fast.html
       //Bresenham circle radius 3
       const ivec2 c[16] = ivec2[16](ivec2( 0, -3), ivec2( 1, -3), ivec2( 2, -2), ivec2( 3, -1),
                                     ivec2( 3,  0), ivec2( 3 , 1), ivec2( 2,  2), ivec2( 1,  3),
                                     ivec2( 0,  3), ivec2(-1,  3), ivec2(-2,  2), ivec2(-3,  1),
                                     ivec2(-3,  0), ivec2(-3, -1), ivec2(-2, -2), ivec2(-1, -3));
       void main() {
           int cx = int(coord.x / dx), cy = int(coord.y / dy);

           float average = textureLod(tex0, coord, lod).r;
           int winD = win + 6; //window + circle diameter      |   ---|------win
           float pix[winD][winD];



           //store all pixel comparisons                        _____________winD
           float thresh = textureLod(tex0, coord, lod).r;      |/    \
           int winD = win + 6; //window + circle diameter      |   ---|------win
           bool cmp[winD][winD];                               |\_|__/circle
           int wd2_1 = -winD / 2, wd2_2 = winD + wd2_1;        |  |
           for (int x = wd2_1; x < wd2_2; x++)                 |  |
               for (int y = wd2_1; y < wd2_2; y++)
                   cmp[cx + x][cy + y] = textureLod(tex0, vec2(float(cx + x) * dx, float(cy + y) * dy), 0).r > thresh;

           //here I use xor operator as "is different than"
           int n_corns = 0;
           ivec2 conrs_loc[win * win];
           float corns_diff[win * win];
           int w2_1 = -win / 2, w2_2 = win + w2_1
           for (int x = w2_1; x < w2_2; x++) {
               for (int y = w2_1; y < w2_2; y++) {
                   int px = cx + x + 3, py = cy + y + 3;
                   //high speed test (check if :
                   if (!(cmp[px][py - 3] ^ cmp[px + 3][py] ^ cmp[px][py + 3] ^ cmp[px - 3][py])) break;
                   //full test:
                   int strk_idx = 0; // n of streaks
                   int streaks[2] = int[2](0, 0);
                   int cur_ctg = 1; // n of contiguous pixels
                   int cur_p = cmp[px, px - 3]; // c[0]
                   for (int i = 1; i < 16; i++) {
                       bool new_p = cmp[px + c[i].x, py + c[i].y]
                       if (cur_p ^ new_p) {
                           if (strk_idx == 2) break;
                           streaks[strk_idx % 2] = cur_ctg;
                           strk_idx++;
                           cur_ctg = 0;
                       }
                       cur_ctg++;
                       cur_p = new_p;
                   }
                   if (strk_idx < 3 ) {
                       streaks[strk_idx
                       if (strk_idx == 2) { // this rejects two valid cases!
                           streaks[0] += streaks[2];
                       if (

                   }
               }
           }
       }
   )glsl");*/

//#define ASENSOR_TYPE_LINEAR_ACCELERATION 10
//#define ASENSOR_TYPE_ROTATION_VECTOR 11
//    ASensorRef linAccSens, orientSens;
//    ASensorEventQueue *sensQueue;
//    bool alive = true;
//    thread([]{
//        ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
//        ASensorManager *sm = ASensorManager_getInstance();
//        linAccSens = ASensorManager_getDefaultSensor(sm, ASENSOR_TYPE_LINEAR_ACCELERATION);
//        orientSens = ASensorManager_getDefaultSensor(sm, ASENSOR_TYPE_ROTATION_VECTOR);
//
//
//        sensQueue = ASensorManager_createEventQueue(sm, looper, 1, nullptr, nullptr);
//        ASensorEventQueue_registerSensor
//        ASensorEventQueue_enableSensor(sensQueue, linAccSens);
//        //ASensorEventQueue_setEventRate(sensQueue, linAccSens, 16000);//8ms -> 120 fps
//        ASensorEventQueue_enableSensor(sensQueue, orientSens);
//        //ASensorEventQueue_setEventRate(sensQueue, orientSens, 16000);
//
//        while (alive) {
//            int id = 0;
//            if ((id = ALooper_pollAll(-1, nullptr, nullptr, nullptr)) >= 0) {
//                // If a sensor has data, process it now.
//                ASensorEvent sensEv;
//                while (ASensorEventQueue_getEvents(sensQueue, &sensEv, 1) > 0) {
//                    if (sensEv.meta_data.sensor == ASENSOR_TYPE_LINEAR_ACCELERATION)
//                        memcpy(accData, sensEv.data, 3 * 4);
//                    else if (sensEv.meta_data.sensor == ASENSOR_TYPE_ROTATION_VECTOR)
//                        memcpy(accData, sensEv.data, 4 * 4);
//                }
//            }
//        }
//    }).detach();