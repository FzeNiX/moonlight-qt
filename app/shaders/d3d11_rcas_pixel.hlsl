// AMD FidelityFX Super Resolution 1.0 Robust Contrast Adaptive Sharpening (RCAS)
// Adapted from ffx_fsr1.h - Copyright (c) 2021 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// This shader is compiled at runtime (see D3D11VARenderer::createSharpenPixelShader()).
// The sharpening strength in [0, 1] comes from a constant buffer so it can be changed live.

cbuffer RCAS_CONST_BUF : register(b1)
{
    float rcasStrength;
    float3 padding;
};

#define RCAS_LIMIT (0.25 - (1.0 / 16.0))

Texture2D<float4> theTexture : register(t0);

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 main(ShaderInput input) : SV_TARGET
{
    uint width, height;
    theTexture.GetDimensions(width, height);

    // Clamp taps to the texture so the edges don't sample black
    int2 maxPos = int2(width, height) - 1;
    int2 p = int2(input.pos.xy);

    float4 e4 = theTexture.Load(int3(p, 0));
    float3 e = e4.rgb;
    float3 b = theTexture.Load(int3(clamp(p + int2( 0, -1), 0, maxPos), 0)).rgb;
    float3 d = theTexture.Load(int3(clamp(p + int2(-1,  0), 0, maxPos), 0)).rgb;
    float3 f = theTexture.Load(int3(clamp(p + int2( 1,  0), 0, maxPos), 0)).rgb;
    float3 h = theTexture.Load(int3(clamp(p + int2( 0,  1), 0, maxPos), 0)).rgb;

    float3 mn4 = min(min(b, d), min(f, h));
    float3 mx4 = max(max(b, d), max(f, h));

    // Limit the sharpening lobe so the output never clips (no ringing)
    float3 hitMin = min(mn4, e) / max(4.0 * mx4, 1e-5);
    float3 hitMax = (1.0 - max(mx4, e)) / min(4.0 * mn4 - 4.0, -1e-5);
    float3 lobeRGB = max(-hitMin, hitMax);
    float lobe = max(-RCAS_LIMIT, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * rcasStrength;

    float3 c = (lobe * (b + d + f + h) + e) / (4.0 * lobe + 1.0);
    return float4(saturate(c), e4.a);
}
