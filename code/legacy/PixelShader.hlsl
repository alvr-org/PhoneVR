
Texture2D leftEye : register(t0);
Texture2D rightEye : register(t1);
SamplerState samp : register(s0);

cbuffer ConstBuf : register(b0) {
    int enableBlur;
    float3 padding;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 coord : TEXCOORD;
};


static const float wr = 0.299f, wb = 0.114f, wg = 1 - wr - wb; //0.587f
static const float ku = 0.436f / (1 - wb), kv = 0.615f / (1 - wr); //0.4921f  0.8773f

static const float wRatio = 1.f / 3.f;
static const float shapeRatio = 2.f / 3.f;

//static const float w0 =, w1, w2, w3 = 1, w4 = 0.05;

float4 blurStep(float x, float y, float off, Texture2D tex) {
    float4 pix = float4(0, 0, 0, 0);
    pix += tex.Sample(samp, float2(x + off, y)) / 8;
    pix += tex.Sample(samp, float2(x + off, y + off)) / 8;
    pix += tex.Sample(samp, float2(x, y + off)) / 8;
    pix += tex.Sample(samp, float2(x - off, y + off)) / 8;
    pix += tex.Sample(samp, float2(x - off, y)) / 8;
    pix += tex.Sample(samp, float2(x - off, y - off)) / 8;
    pix += tex.Sample(samp, float2(x, y - off)) / 8;
    pix += tex.Sample(samp, float2(x + off, y - off)) / 8;
    return pix;
}

float4 rgb2yuv(Texture2D tex, float x, float y) {
    float4 pix = tex.Sample(samp, float2(x, y)) * 0.20;

    if (enableBlur == 1) {
        float cx = x - 0.5, cy = y - 0.5;
        float off = (cx * cx + cy * cy / (shapeRatio * shapeRatio)) / (wRatio * wRatio) / 500;
        pix += blurStep(x, y, off * 1.4, tex) * 0.31;
        pix += blurStep(x, y, off * 3.3, tex) * 0.25;
        pix += blurStep(x, y, off * 5.1, tex) * 0.15;
        pix += blurStep(x, y, off * 7.1, tex) * 0.08;
        pix += blurStep(x, y, off * 9.2, tex) * 0.05;
        pix += blurStep(x, y, off * 11.8, tex) * 0.02;
    }
    else {
        pix /= 0.20;
    }

    float Y = clamp(wr * pix.r + wg * pix.g + wb * pix.b, 0, 1);
    return float4(Y, clamp(ku * (pix.b - Y) + 0.5, 0, 1), clamp(kv * (pix.r - Y) + 0.5, 0, 1), 0);
}

float4 PS(PS_INPUT input) : SV_TARGET{
    float x = input.coord.x * 2;
    float y = input.coord.y;
    if (x < 1.0f)
        return rgb2yuv(leftEye, x, y);
    //else
    return rgb2yuv(rightEye, x - 1.0f, y);
}