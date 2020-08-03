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

struct InstanceData
{
	float4x4 World; 
    float4x4 InvTransWorld;
    float4x4 TexTransform;
    uint selected;
    uint pad0;
    uint pad1;
    uint pad2;
};

StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

Texture2D gDiffuseMap : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);



cbuffer cbPass : register(b0)
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

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer Material : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFrensnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL  : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float3 NormalW : NORMAL;
    float3 PosW : POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint selected : SELECTED;
};


VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout;
	
    InstanceData inst = gInstanceData[instanceID];
	// Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), inst.World);

    vout.PosW = posW.xyz;

    vout.NormalW = mul(float4(vin.NormalL, 0.0f), inst.InvTransWorld).xyz;

    vout.PosH = mul(posW, gViewProj);

    float4 TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform);

    vout.TexC = mul(TexC, inst.TexTransform).xy;

    vout.selected = inst.selected;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{

    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

    diffuseAlbedo.r *= (1 - pin.selected);

    pin.NormalW = normalize(pin.NormalW);
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1 - gRoughness;

    Material mat = { diffuseAlbedo, gFrensnelR0, shininess };

    float3 shadowFactor = 1.0f;

    float4 directLight = computeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = directLight + ambient;

    litColor.a = diffuseAlbedo.a;

    return litColor;
}