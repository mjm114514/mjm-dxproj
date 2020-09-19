static const float PI = 3.1415926535897932f;
cbuffer cbPerFace
{
    float3 gLookAt;
    float padding0;
    float3 gUp;
    float padding1;
    float3 gRight;
    float padding2;
};
TextureCube lightingCube : register(t0);

SamplerState gsamLinearWrap : register(s0);

struct VertexIn
{
    float3 PosL : POSITIONT;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;
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
    vout.PosV.x = gTexCoords[pid].x * 2.0f - 1.0f;
    vout.PosV.y = 1.0f - gTexCoords[pid].y * 2.0f;
    vout.PosV.z = 1.0f;
    vout.PosH = float4(vout.PosV, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normalL = pin.PosV;

    float3 normalW = mul(normalL, float3x3(gRight, gUp, gLookAt));
    normalW = normalize(normalW);

    float3 N = normalW;
    float3 T = cross(normalW, gRight);
    float3 B = cross(T, N);

    T = normalize(T);
    B = normalize(B);

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    float samplerDelta = 0.025f;
    float nrSamplers = 0.0f;

    // convolution on hemisphere
    for (float phi = 0.0f; phi < 2.0 * PI; phi += samplerDelta)
    {
        for (float theta = 0.0f; theta < 0.5 * PI; theta += samplerDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = mul(tangentSample, float3x3(T, B, N));
            irradiance += lightingCube.Sample(gsamLinearWrap, sampleVec) * cos(theta) * sin(theta);
            nrSamplers++;
        }
    }
    irradiance = PI * irradiance * (1.0f / float(nrSamplers));
	return float4(irradiance, 1.0f);
}
