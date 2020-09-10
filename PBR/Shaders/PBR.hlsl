static const float PI = 3.14159265f;

#include "Common.hlsl"

float DistributionGGX(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = NdotH2 * (a2 - 1) + 1;
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float k)
{
    float NdotV = max(dot(N, V), 0);
    float NdotL = max(dot(N, L), 0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1 - f0) * pow(1 - cosTheta, 5);
}

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
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    vout.PosH = mul(posW, gViewProj);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3) gInvTransWorld);

    vout.TexC = vin.TexC;

    return vout;
};

float4 PS(VertexOut pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);
    float3 V = normalize(gEyePosW - pin.PosW);

    MaterialData Mat = gMaterialData[gMaterialIndex];

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, Mat.albedo, Mat.metallic);

    float3 light = float3(0, 0, 0);

    for (int i = 0; i < 4; i++)
    {
        float3 toEye = gLights[i].dir_and_pos - pin.PosW;
        float3 L = normalize(toEye);
        float3 H = normalize(L + V);

        float distance = length(toEye);
        float attenuation = 1 / (distance * distance);
        float3 radiance = gLights[i].lightColor * attenuation;

        float cosTheta = max(dot(H, V), 0.0);
        float3 F = fresnelSchlick(cosTheta, F0);

        float NDF = DistributionGGX(N, H, Mat.roughness);
        float G = GeometrySmith(N, V, L, Mat.roughness);

        float3 numerator = NDF * G * F;
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        float3 specular = numerator / max(denominator, 0.001);

        float3 kD = (float3(1, 1, 1) - F) * (1 - Mat.metallic);

        float NdotL = max(dot(N, L), 0.0);
        light += (kD * Mat.albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * Mat.albedo * Mat.ao;

    float3 color = ambient + light;
    color = color / (color + float3(1, 1, 1));

    return float4(color, 1);
}

