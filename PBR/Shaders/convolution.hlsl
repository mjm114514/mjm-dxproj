#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITIONT;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float3 PosH : SV_Position;
    float3 PosL : POSITIONT;
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
