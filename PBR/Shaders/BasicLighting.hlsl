#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITIONT;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    vout.PosH = mul(posW, gViewProj);
    vout.PosW = posW.xyz;

    return vout;
};

float4 PS(VertexOut pin) : SV_Target
{
    float ambientStrength = 0.1;
    float3 ambientLight = gLights[0].lightColor * ambientStrength;

    float4 diffuseAlbedo = gMaterialData[gMaterialIndex].DiffuseAlbedo;

    float3 result = ambientLight * diffuseAlbedo.rgb;

    return float4(result, diffuseAlbedo.a);
}
