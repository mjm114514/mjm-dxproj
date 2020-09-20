#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITIONT;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

VertexOut VS(uint pid : SV_VertexID)
{
    VertexOut vout;

    vout.TexC = gTexCoords[pid];
    vout.PosH.x = vout.TexC.x * 2 - 1;
    vout.PosH.y = 1 - vout.TexC.y * 2;
    vout.PosH.z = 1.0f;
    vout.PosH.w = 1.0f;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return float4(gLUTMap.Sample(gsamLinearWrap, pin.TexC).rg, 0, 1);
}
