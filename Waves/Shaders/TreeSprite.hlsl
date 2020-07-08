#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "lightingUtil.hlsl"

Texture2DArray gDiffuseMapArray : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
 
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld; 
    float4x4 gInvTransWorld;
    float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;

    float4 gAmbientLight;

    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer Material : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFrensnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosW  : POSITION;
    float2 SizeW : SIZE;
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 SizeW : SIZE;
};

struct GeoOut{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    uint PrimID : SV_PrimitiveID;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

    vout.CenterW = vin.PosW;
    vout.SizeW = vin.SizeW;
	
    return vout;
}

// Expand each point into a quad.

[maxvertexcount(4)]
void GS(point VertexOut gin[1], uint primID : SV_PrimitiveID, inout TriangleStream<GeoOut> triStream){
    float3 up =  float3(0.0f, 1.0f, 0.0f);
    float3 toEye = gEyePosW - gin[0].CenterW;
    toEye.y = 0.0f;
    toEye = normalize(toEye);
    float3 right = cross(up, toEye);

    float halfWidth = 0.5f * gin[0].SizeW.x;
    float halfHeight = 0.5f * gin[0].SizeW.y;

    float4 v[4];
    v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(gin[0].CenterW - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);

    float2 tex[4] = {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f),
    };
    GeoOut gout;
    [unroll]
    for(int i = 0; i < 4; i++){
        gout.PosH = mul(v[i], gViewProj);
        gout.PosW = v[i].xyz;
        gout.NormalW = toEye;
        gout.TexC = tex[i];
        gout.PrimID = primID;

        triStream.Append(gout);
    }
}

float4 PS(GeoOut pin) : SV_Target
{
    float3 uvw = float3(pin.TexC, pin.PrimID % 3);
    float4 diffuseAlbedo = gDiffuseMapArray.Sample(gsamAnisotropicWrap, uvw) * gDiffuseAlbedo;

#ifdef ALPHA_TEST
    // discard pixel if texture alpha < 0.01. We do this test ASAP in the shader so that we can potentially exit the shader early.
    // thereby skipping the rest of code.
    clip(diffuseAlbedo.a - 0.01f);
#endif
    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = gEyePosW - pin.PosW;

    float distToEye = length(toEyeW);

    toEyeW /= distToEye;

    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1 - gRoughness;

    Material mat = { diffuseAlbedo, gFrensnelR0, shininess };

    float3 shadowFactor = 1.0f;

    float4 directLight = computeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = directLight + ambient;

#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    litColor.a = diffuseAlbedo.a;

    return litColor;
}
