
//////////////////OBIETTIVO////////////
// https://scontent.oculuscdn.com/t64.5771-25/12386390_1804207246535167_5682105534900076544_n.gif/FrameExtrapolationOptimizedForGif.gif


/* LA MIA TESI: RISOLVERE L'"OPTICAL FLOW" PER APPLICAZIONI DI TIPO PERCETTIVO
 * I e' un tensore (sequenza di immagini), I_tau e' la matrice tau-esima.
 *
 * Dato un punto p e due matrici I_0 e I_1, l'equazione del flusso da I_0 a I_1:
 *
 *           I_x * u + I_y * v = I_t
 *
 * dove I_x e' la derivata parziale rispetto a x di I_0 in p,
 * I_y e' la derivata parziale rispetto a y di I_0 in p,
 * I_t e' la derivata parziale rispetto al tempo di I in p (in pratica I_1(p) - I_0(p))
 * u e' la componente orizzontale del flusso in I_0(p),
 * v e' la componente verticale del flusso in I_0(p).
 *
 * Presupposto (Lucas-Kanade): I e' derivabile in x, y, t, quindi I_tau(p) ~= I_tau(p + (dx, dy)) ~= I_(tau + dt)(p), per ogni p, tau ("local smoothness" spaziale e temporale)
 * Quindi posso invertire I_0 e I_1 e cambia solo segno di u e v per ogni p.
 *
 * Lucas-Kanade: usando il "local smoothness" risolvo il sistema sovradeterminato
 *         I_x(p + w) * u + I_y(p + w) * v = I_t(p + w)
 * con w appartenente a [-eps, eps]x[-eps, eps] {eps := valore sufficientemente piccolo}
 * Resto dell'algoritmo su https://en.wikipedia.org/wiki/Lucas%E2%80%93Kanade_method
 * Se nelle immagini sono presenti traslazioni superiori ad un pixel e' necessario ricorrere ad un metodo per il calcolo del flusso globale
 * In questo caso ho usato un metodo iterativo calcolando Lucas-Kanade su piu' livelli di dettaglio (lod). NB: in opengl, la dimensione delle mipmap e' dim / 2^lod
 *   Calcolo il flusso per il livello piu' alto (risoluzione piu' bassa)
 *   Per ogni altro livello, a cascata, traslo I_1 di (-u, -v) * 2^lod e ricalcolo il flusso
 * Ad ogni iterazione viene migliorata la stima di u e v.
 *
 * PROBLEMA: il flusso calcolato con l'algoritmo di Lucas-Kanade e' stabile solo quando in I(p) e' presente un gradiente pronunciato.
 * Nelle aree corrispondenti a bordi e zone piatte Lucas-Kanade diventa molto instabile, spesso generando in pratica divisioni per zero.
 *
 * //////////RISOLUZIONE: I TENTATIVO ////////////
 *  Uso il determinante della matrice A di Harris ("cornessness") (stessa matrice A usata in Lucas-Kanade)
 * [vedi https://en.wikipedia.org/wiki/Harris_affine_region_detector   https://www.youtube.com/watch?v=vkWdzWeRfC4 ]
 * come coefficiente/peso per interpolare il risultato di Lucas-Kanade con quello di un metodo per i bordi ad hoc.
 *
 * Se nell'equazione del flusso impongo che (u,v) sia parallelo alla normale del bordo ottengo:
 *      u = I_t * I_x / (I_x^2 + I_y^2)    v = I_t * I_x / (I_x^2 + I_y^2)
 *
 * RISULTATO: Lucas-Kanade viene sostanzialmente ignorato; l'algoritmo per i bordi riesce a trovare il flusso per gradienti molto piccoli
 * ma il risultato e' complessivamente peggiore a causa del rumore elevato.
 *
 * ///// II TENTATIVO //////////
 * Uso il determinante della matrice di Harris come peso nell'applicazione di un blur gaussiano.
 * RISULTATO: Nessun miglioramento.

*/


#include <jni.h>
#include "StrUtils.h"
#include "RenderUtils.h"

#define FUNC(type, func) extern "C" JNIEXPORT type JNICALL Java_com_example_ricca_spacewarptest_MainActivity_##func
#define SUB(func) FUNC(void, func)

using namespace std;

namespace {
    // change these
    const int MIN_LOD = 3;
    const int MAX_LOD = 6;

    const int camW = 1920, camH = 1080;
    int widths[MAX_LOD + 1], heights[MAX_LOD + 1];
    float wf[MAX_LOD + 1], hf[MAX_LOD + 1], dw[MAX_LOD + 1], dh[MAX_LOD + 1];

    GLuint camTex;
    GLuint lkfProg, compProg, xGaussProg, yGaussProg;
    GLint flowMulUnif, minLodUnif, maxLodUnif, xLodUnif, yLodUnif;

    Renderer *texRdr[3], *shotRdr[2],
            *grayRdr, *preSmoothRdr[MIN_LOD],
            *derivRdr, *derivPyrRdr[MAX_LOD - MIN_LOD],
            *LKFlowRdr[MAX_LOD - MIN_LOD + 1], *xGaussRdr, *yGaussRdr,
            *compRdr, *flowVisualRdr;
}

FUNC(jint, InitGL)(JNIEnv *, jobject) {

    // calculate size constants
    for (int i = 0; i < MAX_LOD + 1; ++i) {
        int lodMul = pow(2, i);
        widths[i] = camW / lodMul; // round down
        heights[i] = camH / lodMul;
        wf[i] = widths[i];
        hf[i] = heights[i];
        dw[i] = 1.f / wf[i];
        dh[i] = 1.f / hf[i];
    }

    camTex = genTexture(true);
    shotRdr[0] = new Renderer(camW, camH, {{camTex, true}}, FS_PT, GL_RGBA8, GL_NEAREST);
    shotRdr[1] = new Renderer(camW, camH, {{camTex, true}}, FS_PT, GL_RGBA8, GL_NEAREST);
    texRdr[0] = new Renderer({{camTex, true}});
    texRdr[1] = new Renderer({{shotRdr[0]->target(), false}});
    texRdr[2] = new Renderer({{shotRdr[1]->target(), false}});


    //http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
    // calculate perfectly gauss smoothered downsampled textures using linear sampling,
    // for each pixel of previous level, sample with an x, y offset of 0.25 * (dw,dh) away from the center of the pixel of the next level.
    // then calculate 1 level of mipmap. if the offset is 0.5, the smooth becomes a box filter.
    // final cost: 1 tap + 1 mipmap per level, equivalent to 1 mipmap + 9 taps, but even more precise.
    // NB: mipmap is equivalent to 1 tap exactly between 4 pixels
    string gauss1tapFS = crypt(R"glsl(
        const float dw = %f, dh = %f;
        const float dw2 = dw * 2., dh2 = dh * 2.;

        void main(){
            color = vec4(textureLod(tex0, coord + vec2(mod(coord.x, dw2) - dw, mod(coord.y, dh2) - dh) / 2., %s);
            // the missing paren is added with %%s
        }
    )glsl");

    grayRdr = new Renderer(camW, camH, {{shotRdr[0]->target(), false}, {shotRdr[1]->target(), false}}, str_fmt(crypt(R"glsl(
        float gray(sampler2D tex) {
            vec3 p = texture(tex, coord).rgb;
            return p.r * .2125 + p.g * .7154 + p.b * .0721;
        }
        void main(){
            color = vec4(gray(tex1), gray(tex0), 0, 0);
        }
    )glsl")), GL_RG16_EXT, GL_LINEAR);


    auto prevTex = grayRdr->target();
    for (int i = 0; i < MIN_LOD; i++) {
        preSmoothRdr[i] = new Renderer(widths[i], heights[i], {{prevTex, false}},
                                       str_fmt(gauss1tapFS, dw[i], dh[i], (string(i == 0 ? "0" : "1") + ".).rg, 0, 0").c_str()), //use lod 0 for first pass, then 1.
                                       GL_RG16_EXT, GL_LINEAR, 0, 1); //1 level of mipmap!
        prevTex = preSmoothRdr[i]->target();
    }

    derivRdr = new Renderer(widths[MIN_LOD], heights[MIN_LOD], {{prevTex, false}}, str_fmt(crypt(R"glsl(
        const vec2 xoff = vec2(%f / 2., 0.), yoff = vec2(0., %f / 2.); // divide by 2 -> sample between pixels -> avoid / 2 later

        void main(){
            vec2 p0 = textureLod(tex0, coord, 1.).rg;
            color = vec4(textureLod(tex0, coord + xoff, 1.).g - textureLod(tex0, coord - xoff, 1.).g + .5,
                         textureLod(tex0, coord + yoff, 1.).g - textureLod(tex0, coord - yoff, 1.).g + .5, p0.g, p0.r);
        }
    )glsl"), dw[MIN_LOD], dh[MIN_LOD]), GL_RGBA16_EXT, GL_LINEAR);
    //output: {Ix + .5, Ix + .5, I1, I0);     using symmetric differentiation

    prevTex = derivRdr->target();
    for (int i = MIN_LOD; i < MAX_LOD; i++) {
        derivPyrRdr[i - MIN_LOD] = new Renderer(widths[i], heights[i], {{prevTex, false}},
                                       str_fmt(gauss1tapFS, dw[i], dh[i], i == 0 ? "0.)" : "1.)"),
                                       GL_RGBA16_EXT, GL_LINEAR, 0, 1); //1 level of mipmap!
        prevTex = derivPyrRdr[i - MIN_LOD]->target();
    }

    for (int i = MAX_LOD; i >= MIN_LOD; i--) {
        vector<pair<GLuint, bool>> texs;
        if (i != MIN_LOD)
            texs.emplace_back(derivPyrRdr[i - MIN_LOD - 1]->target(), false);
        else
            texs.emplace_back(derivRdr->target(), false);
        if (i != MAX_LOD)
            texs.emplace_back(LKFlowRdr[i - MIN_LOD + 1]->target(), false);

        // do 4 taps between the 4 vertices of the current pixel.
        // the equivalent kernel is:    0.25  0.5  0.25
        //                              0.5    1   0.5
        //                              0.25  0.5  0.25

        LKFlowRdr[i - MIN_LOD] = new Renderer(widths[i], heights[i], texs, str_fmt(/*crypt*/(R"glsl(
            const float dw = %f, dh = %f;
            //const vec2(
            //float lodMul = %%f;

            void main() {
                //float corn = 0.; // cornerness
                vec2 oldCoord = coord;
        #if %i
                vec2 oldFlow = (texture(tex1, coord).xy - .5) * 2.;
                oldCoord -= vec2((floor(oldFlow.x / dw) + .5) * dw, (floor(oldFlow.y / dh) + .5) * dh); // allineate to nearest pixel
        #endif
                float sIxIx = 0., sIyIy = 0., sIxIy = 0., sIxIt = 0., sIyIt = 0.;
                for (float x = -dw / 2.; x < dw; x += dw) {
                    for (float y = -dh / 2.; y < dh; y += dh) {
                        vec2 off = vec2(x, y);
                        vec3 p = textureLod(tex0, coord + off, 1.).xyz;
                        float Ix = p.x - .5, Iy = p.y - .5, It = p.z - textureLod(tex0, oldCoord + off, 1.).w;
                        sIxIx += Ix * Ix;
                        sIyIy += Iy * Iy;
                        sIxIy += Ix * Iy;
                        sIxIt += Ix * It;
                        sIyIt += Iy * It;
                    }
                }
                float det = max((sIxIx * sIyIy - sIxIy * sIxIy), 0.00001);
                vec2 flow = vec2((sIxIt * sIyIy - sIyIt * sIxIy) * dw,
                                 (sIyIt * sIxIx - sIxIt * sIxIy) * dh) / det;
                color = vec4((coord - oldCoord - normalize(flow) * min(length(flow), 1.)) / 2. + .5 , 0, 1);
                             //max(det, 0.00002) / lodMul / 2., 1);
            }
        )glsl"), dw[i], dh[i], int(i != MAX_LOD)), GL_RGBA16_EXT, GL_LINEAR);
        //output: flow (x, y) in pixels / 4 / (w, h) + .5
    }


    // http://dev.theomader.com/gaussian-kernel-calculator/
    string gaussFS = crypt(R"glsl(
        const float dw = %f, dh = %f;

        const int taps = 20;

        void main() {
            vec2 pixs[taps];
            vec2 off = -(float(taps) + .5) * vec2(dw, dh);
            float weightTot = 0.;
            for (int i = 0; i < taps; i++) {
                vec3 p = texture(tex0, coord + off).xyz;
                float weight = p.z * 1.; //g[i]
                pixs[i] = p.xy * weight;
                weightTot += weight;
                off += 2. * vec2(dw, dh);
            }
            vec2 tot = vec2(0, 0);
            for (int i = 0; i < taps; i++)
                tot += pixs[i];
            color = vec4(tot / weightTot, weightTot, 1);
        }
    )glsl");
    xGaussRdr = new Renderer(widths[MIN_LOD], heights[MIN_LOD], {{LKFlowRdr[0]->target(), false}}, str_fmt(gaussFS, dw[MIN_LOD], 0.f), GL_RGBA16_EXT, GL_LINEAR);
    yGaussRdr = new Renderer(widths[MIN_LOD], heights[MIN_LOD], {{xGaussRdr->target(), false}}, str_fmt(gaussFS, 0.f, dh[MIN_LOD]), GL_RGBA16_EXT, GL_LINEAR);

    compRdr = new Renderer({{LKFlowRdr[0]->target(), false}, {shotRdr[1]->target(), false}}, crypt(R"glsl(
        uniform float flowMul;

        void main(){
            color = texture(tex1, coord + (texture(tex0, coord).xy - .5) * 4. * flowMul);
        }
    )glsl"));
    compProg = compRdr->getProg();
    flowMulUnif = glGetUniformLocation(compProg, "flowMul");

    flowVisualRdr = new Renderer({{derivRdr->target(), false}}, crypt(R"glsl(
        float cl(float f) { return clamp(f, 0., 1.); }

        void main(){
            vec2 flow = (texture(tex0, coord).xy - .5);// * 100. ;
            color = vec4((cl(flow.x) + cl(flow.y)) / 2., (cl(flow.y) + cl(-flow.x)) / 2., cl(-flow.y), 1);
        }
    )glsl"));


    return camTex;
}

SUB(Draw)(JNIEnv *, jobject, int w, int h, int which, float flowMul) {
    if (which > 2) {
        grayRdr->render();
        for (int i = 0; i < MIN_LOD; i++)
            preSmoothRdr[i]->render();
        derivRdr->render();
        for (int i = 0; i < MAX_LOD - MIN_LOD; i++)
            derivPyrRdr[i]->render();
        for (int i = MAX_LOD - MIN_LOD; i >= 0; i--)
            LKFlowRdr[i]->render();
//
//        xGaussRdr[0]->render();
//        yGaussRdr[1]->render();
    }

    setupRTS(w, h);

    if (which == 3) {
        glUseProgram(compProg);
        glUniform1f(flowMulUnif, flowMul);
        compRdr->render();
    }
    else if (which == 4)
        flowVisualRdr->render();
    else
        texRdr[which]->render();
}

SUB(Shot)(JNIEnv *, jobject, int which) {
    shotRdr[which]->render();
}





// (9.5 is the sum of window weights)
//const float weightsSqr = 90.25;
// flow = LK flow * det + edge flow * (1-det)
//float det_trace = (1. - (sIxIx * sIyIy - sIxIy * sIxIy) / weightsSqr) / max(sIxIx + sIyIy, .000001);
//newCoord += vec2(max(min((sIxIt * sIyIy - sIyIt * sIxIy) / weightsSqr + sIxIt * det_trace, lodMul), -lodMul) / w,
//max(min((sIyIt * sIxIx - sIxIt * sIxIy) / weightsSqr + sIyIt * det_trace, lodMul), -lodMul) / h);