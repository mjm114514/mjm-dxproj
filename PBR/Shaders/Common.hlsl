#define MAX_LIGHT 16
// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

struct Light
{
    float3 dir_and_pos;
    float padding0;
    float3 lightColor;
    float padding1;
};

// Include structures and functions for lighting.
struct MaterialData
{
    float3 albedo;
    float metallic;

    float roughness;
    float ao;
    int albedoMapIndex;
    int metallicMapIndex;

    int roughnessMapIndex;
    int aoMapIndex;
    int normalMapIndex;
    float padding;
};

TextureCube gCubeMap : register(t0);
TextureCube gIrradianceMap : register(t1);
Texture2D gTextureMaps[20] : register(t2);

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvTransWorld;
	float4x4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MAX_LIGHT];
};

// float3 CalcDirLight(Light light, float3 normal, float3 toEye, float3 diffuseColor, float3 specularColor, float shininess)
// {
//     MaterialData Mat = gMaterialData[gMaterialIndex];
// 
//     float3 lightDir = normalize(-light.dir_and_pos);
// 
//     float diff = max(dot(lightDir, normal), 0.0f);
// 
//     float3 reflectDir = reflect(-lightDir, normal);
//     float spec = pow(max(dot(reflectDir, toEye), 0.0f), shininess);
// 
//     float3 ambient = light.ambient * diffuseColor;
//     float3 diffuse = light.diffuse * diff * diffuseColor;
//     float3 specular = light.specular * spec * specularColor;
// 
//     return ambient + diffuse + specular;
// }
// 
// float3 CalcPointLight(Light light, float3 normal, float3 posW, float3 diffuseColor, float3 specularColor, float shininess)
// {
//     MaterialData Mat = gMaterialData[gMaterialIndex];
// 
//     float3 lightDir = normalize(light.dir_and_pos - posW);
//     float3 toEye = normalize(gEyePosW - posW);
// 
//     float diff = max(dot(lightDir, normal), 0.0f);
// 
//     float3 reflectDir = reflect(-lightDir, normal);
//     float spec = pow(max(dot(reflectDir, toEye), 0.0f), shininess);
// 
//     float distance = length(light.dir_and_pos - posW);
//     float attenuation = 1.0f / (light.k_constant + light.k_linear * distance + light.k_quad * distance * distance);
// 
//     float3 ambient = light.ambient * diffuseColor;
//     float3 diffuse = light.diffuse * diff * diffuseColor;
//     float3 specular = light.specular * spec * specularColor;
// 
//     return (ambient + diffuse + specular) * attenuation;
// }
