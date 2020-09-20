static float PI = 3.14159265358979323846;

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

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0);
    float NdotL = max(dot(N, L), 0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float2 integrateBRDF(float NdotV, float Roughness)
{
    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0;
    V.z = NdotV;

    float A = 0;
    float B = 0;

    float3 N = float3(0, 0, 1);
    
    const int numSampler = 1024;
    for (uint i = 0; i < numSampler; i++)
    {
        float2 Xi = Hammersley(i, numSampler);
        float3 H = importanceSampleGGX(Xi, Roughness);
        float3 L = 2 * dot(V, H) * H - V;

        float NdotL = max(L.z, 0);
        float VdotH = max(dot(V, H), 0);
        float NdotH = max(H.z, 0);

        if (NdotL > 0)
        {
            float G = GeometrySmith(N, V, L, Roughness);
            float G_vis = G * VdotH / (NdotH * NdotV);
            float fc = pow(1 - VdotH, 5.0);

            A += (1.0 - fc) * G_vis;
            B += fc * G_vis;
        }
    }
    A /= numSampler;
    B /= numSampler;
    return float2(A, B);
}

struct VertexIn
{
    float3 PosL : POSITION;
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
    vout.PosH.x = gTexCoords[pid].x * 2.0f - 1.0f;
    vout.PosH.y = 1.0f - gTexCoords[pid].y * 2.0f;
    vout.PosH.z = 1.0f;
    vout.PosH.w = 1.0f;
    vout.TexC = gTexCoords[pid];
    return vout;
}

float2 PS(VertexOut pin) : SV_Target
{
    float NdotV = pin.TexC.x;
    float Roughness = pin.TexC.y;
    
    return integrateBRDF(NdotV, Roughness);
}
