static float PI = 3.1415926535897932f;
cbuffer cbPerFace
{
    float3 gLookAt;
    float padding0;
    float3 gUp;
    float padding1;
};
TextureCube lightingCube : t0;

SamplerState gsamLinearWrap : register(s0);

struct VertexIn
{
    float3 PosL : POSITIONT;
};

struct VertexOut
{
    float3 PosH : SV_Position;
    float3 TexC : TEXCOORD;
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

VertexOut VS(int pid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = gTexCoords[i];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 gRight = normalize(cross(gUp, gLookAt));
    float3 normalL = float3(pin.PosH.xy, 1.0f);
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
