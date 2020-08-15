#define MaxLights 16

struct Material{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    // Shiness = 1 - roughness
    float shiness;
};

struct Light{
    float3 Strength;
    float FalloffStart; // point/spot light only
    float3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    float3 Position;    // point light only
    float SpotPower;    // spot light only
};


float calcAttenuation(float d, float falloffEnd, float falloffStart){
    // Linear falloff
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 schlickFresnel(float3 R0, float3 normal, float3 lightVec){
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1 - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflectPercent;
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat){
    const float m = mat.shiness * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);
    float3 FresnelFactor = schlickFresnel(mat.FresnelR0, normal, lightVec);
    float roughnessFactor = (m + 8.0f) * pow(max(dot(normal, halfVec), 0.0f), m) / 8.0f;

    float3 specAlbedo = FresnelFactor*roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 computeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye){
    float3 lightVec = -L.Direction;
    float3 lambertCos = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * lambertCos;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 computePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye){
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    lightVec /= d;

    if(d > L.FalloffEnd)
        return 0.0f;

    float lambertCos = max(dot(normal, lightVec), 0.0f);
    float3 lightStrength = lambertCos * calcAttenuation(d, L.FalloffEnd, L.FalloffStart) * L.Strength;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 computeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye){
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if(d > L.FalloffEnd)
        return 0.0f;
    
    lightVec /= d;

    float lambertCos = max(dot(lightVec, normal), 0.0f);

    float3 lightStrength = lambertCos * calcAttenuation(d, L.FalloffEnd, L.FalloffStart) * L.Strength;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);

    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 computeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor){

    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i){
        result += shadowFactor[i] * computeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i){
        result += computePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i){
        result += computeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}