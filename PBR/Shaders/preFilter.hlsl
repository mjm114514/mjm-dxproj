static const float PI = 3.1415926535897932f;
cbuffer cbPerFace : register(b0)
{
    float3 gLookAt;
    float padding0;
    float3 gUp;
    float padding1;
    float3 gRight;
    float padding2;
};

cbuffer cbPerCube : register(b1)
{
    float gRoughness;
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

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverseVdC(i));
}

float3 importanceSampleGGX(float2 Xi, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordiantes
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    return H;
}

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

    float3 PrefilteredColor = float3(0, 0, 0);

    float3 V = N;

    const uint numSamplers = 1024;
    float totalWeight = 0;

    for (uint i = 0; i < numSamplers; i++)
    {
        float2 Xi = Hammersley(i, numSamplers);
        float3 H = importanceSampleGGX(Xi, gRoughness);

        H = mul(H, float3x3(T, B, N));
        float3 L = reflect(-V, H);

        float NdotL = max(dot(N, L), 0);
        if (NdotL > 0)
        {
            PrefilteredColor += lightingCube.SampleLevel(gsamLinearWrap, L, 0.0f).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    return float4(PrefilteredColor / totalWeight, 1.0f);

}
