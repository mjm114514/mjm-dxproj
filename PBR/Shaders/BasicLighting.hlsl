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
    float3 normal = normalize(pin.NormalW);
    float3 lightDir = normalize(gLights[0].lightPos - pin.PosW);

    MaterialData Mat = gMaterialData[gMaterialIndex];

    float4 diffuseColor = gTextureMaps[Mat.DiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

    float3 ambient = gLights[0].ambient * diffuseColor.rgb;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = gLights[0].diffuse * diff * diffuseColor.rgb;

    // Specular
    float3 toEye = normalize(gEyePosW - pin.PosW);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(toEye, reflectDir), 0.0f), Mat.Shininess);
    
    float3 specular = gLights[0].specular * (spec * Mat.Specular);

    float3 result = ambient + diffuse + specular;

    return float4(result, diffuseColor.a);
}
