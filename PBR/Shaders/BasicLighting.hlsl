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
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    vout.PosH = mul(posW, gViewProj);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3) gInvTransWorld);

    return vout;
};

float4 PS(VertexOut pin) : SV_Target
{

    // calculate diffuse light
    float3 normal = normalize(pin.NormalW);
    float3 lightDir = normalize(gLights[0].lightPos - pin.PosW);

    float diff = max(dot(normal, lightDir), 0.0f);

    float3 diffuseLight = diff * gLights[0].lightColor;

    // calculate ambient light
    float ambientStrength = 0.1;
    float3 ambientLight = gLights[0].lightColor * ambientStrength;

    float4 diffuseAlbedo = gMaterialData[gMaterialIndex].DiffuseAlbedo;

    // calculate specular light
    float specularStrength = 0.5;
    float3 toEye = normalize(gEyePosW - pin.PosW);
    float3 reflectDir = reflect(-lightDir, normal);

    float spec = pow(max(dot(toEye, reflectDir), 0.0f), 32);
    float3 specular = specularStrength * spec * gLights[0].lightColor;

    float3 result = (ambientLight + diffuseLight + specular) * diffuseAlbedo.rgb;

    return float4(result, diffuseAlbedo.a);
}
