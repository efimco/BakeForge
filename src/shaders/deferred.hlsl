
#include "BRDF.hlsl"
#include "constants.hlsl"
Texture2D tAlbedo : register(t0);
Texture2D tMetallicRoughness : register(t1);
Texture2D tNormal : register(t2);
Texture2D tFragPos : register(t3);
Texture2D<uint> tObjectID : register(t4);
Texture2D tDepth : register(t5);

struct Light
{
    int lightType;
    float3 position;
    float intensity;
    float3 color;
    float3 direction;
    float padding1; // Padding to align to 16 bytes
    float2 spotParams;
    float2 padding2; // Additional padding to align to 16 bytes
    float3 attenuations;
    float radius; // radius for point lights
};

cbuffer CB : register(b0)
{
    float IBLrotationY;
    float IBLintensity;
    float2 padding; // Padding to align to 16 bytes
    float3 cameraPosition;
    float padding2; // Padding to align to 16 bytes
};

StructuredBuffer<Light> lights : register(t6);
Texture2D<float4> tBackground : register(t7);
TextureCube tIrradianceMap : register(t8);
TextureCube tPrefilteredMap : register(t9);
Texture2D tBRDFLUT : register(t10);

SamplerState linearSampler : register(s0);
RWTexture2D<unorm float4> outColor : register(u0);



static const float MAX_REFLECTION_LOD = 7.0;
static const float MIN_REFLECTANCE = 0.04;
static const float YAW = PI / 180.0;

float3x3 getIrradianceMapRotation()
{
    float angle = IBLrotationY * YAW;
    float cosA = cos(angle);
    float sinA = sin(angle);
    return float3x3(cosA, 0.0, -sinA, 0.0, 1.0, 0.0, sinA, 0.0, cosA);
}

float linear_rgb_to_srgb(float color) // took it from blender's source code
{
    if (color < 0.0031308)
    {
        return (color < 0.0) ? 0.0 : color * 12.92;
    }

    return 1.055 * pow(color, 1.0 / 2.4) - 0.055;
}

float3 aces(float3 color)
{
    return color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14);
}

float3 agxDefaultContrastApprox(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;

    return +15.5 * x4 * x2
        - 40.14 * x4 * x
        + 31.96 * x4
        - 6.868 * x2 * x
        + 0.4298 * x2
        + 0.1191 * x
        - 0.00232;
}

float3 agx(float3 color)
{
    const float3x3 agxMat = float3x3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );

    const float3x3 agxMatInv = float3x3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116
    );

    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    color = mul(agxMat, color);
    color = max(color, 1e-10);
    color = log2(color);
    color = (color - minEv) / (maxEv - minEv);
    color = clamp(color, 0.0, 1.0);
    color = agxDefaultContrastApprox(color);
    color = mul(agxMatInv, color);

    return color;

}

uint querySpecularTextureLevels()
{
    uint width, height, levels;
    tPrefilteredMap.GetDimensions(0, width, height, levels);
    return levels;
}
float3 applyPointLight(Light light, float3 fragPos, float3 N, float3 V, float3 F0, float roughness, float3 albedo, float metallic)
{
    float3 L = normalize(light.position - fragPos);
    float3 H = normalize(V + L);

    float distance = length(light.position - fragPos);

    // Attenuation
    float attenuation = saturate(pow(1.0 - pow(distance / light.radius, 4.0), 2.0)) / (pow(distance, 2.0) + 1.0);

    if (light.radius < distance)
    {
        attenuation = 0.0;
    }

    float3 radiance = light.color * light.intensity * attenuation;

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    float3 F = fresnelSchlick(HdotV, F0, roughness);

    // Energy conservation
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    // Specular
    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.001; // Add epsilon to prevent divide by zero
    float3 specular = numerator / denominator;

    float3 result = (kD * albedo / PI + specular) * radiance * NdotL;

    return result;
}

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
    float3 fragPos = tFragPos.Load(int3(DTid.xy, 0)).xyz;
    float3 normal = tNormal.Load(int3(DTid.xy, 0)).xyz; // encoded in [0,1]
    float4 albedo = tAlbedo.Load(int3(DTid.xy, 0));
    float2 metallicRoughness = tMetallicRoughness.Load(int3(DTid.xy, 0)).xy;
    uint objectID = tObjectID.Load(int3(DTid.xy, 0));
    float depth = tDepth.Load(int3(DTid.xy, 0)).x;
    float3 background = tBackground.Load(int3(DTid.xy, 0)).xyz;

    float3 N = normalize(normal * 2.0f - 1.0f);

    float metallic = saturate(metallicRoughness.x);
    float roughness = saturate(metallicRoughness.y);

    // Early exit for background
    if (objectID == 0)
    {
        background.r = linear_rgb_to_srgb(background.r);
        background.g = linear_rgb_to_srgb(background.g);
        background.b = linear_rgb_to_srgb(background.b);
        outColor[DTid.xy] = float4(background, 1.0);
        return;
    }

    float3 V = normalize(cameraPosition - fragPos);
    float NdotV = clamp(dot(N, V), 0.01, 0.99);

    float3 R = reflect(-V, N);

    // Apply IBL rotation
    float3x3 irradianceRotation = getIrradianceMapRotation();
    float3 rotatedN = mul(irradianceRotation, N);
    float3 rotatedR = mul(irradianceRotation, R);

    // Metallic workflow: interpolate between dielectric 0.04 and albedo by metallic
    float3 F0 = lerp(float3(MIN_REFLECTANCE, MIN_REFLECTANCE, MIN_REFLECTANCE), albedo.rgb, metallic);

    // IBL Diffuse
    float3 irradiance = tIrradianceMap.SampleLevel(linearSampler, rotatedN, 0).rgb;
    float3 F = fresnelSchlick(NdotV, F0, roughness);
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    // Lambert diffuse with irradiance convolution already accounting for 1/PI; omit /PI if irradiance is pre-integrated.
    float3 diffuseIBL = irradiance * albedo.rgb * kD;

    // IBL Specular
    uint numMips = querySpecularTextureLevels();  // total mip levels
    float mipLevel = roughness * max(float(numMips) - 1.0f, 0.0f);
    float3 prefilteredColor = tPrefilteredMap.SampleLevel(linearSampler, rotatedR, mipLevel).rgb;
    float2 brdf = tBRDFLUT.SampleLevel(linearSampler, float2(NdotV, roughness), 0).rg;
    float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

    // Combine IBL
    float3 finalColor = (diffuseIBL + specularIBL) * IBLintensity;

    for (int i = 0; i < lights.Length; ++i)
    {
        Light light = lights[i];
        if (light.lightType == 0) // Point
        {
            finalColor += applyPointLight(light, fragPos, N, V, F0, roughness, albedo.rgb, metallic);
        }
        else if (light.lightType == 1) // Directional
        {
            continue;
        }
        else if (light.lightType == 2) // Spot
        {
            continue;
        }

    }
    // finalColor = lights[0].intensity;

    // Gamma correction (Blender uses 2.2, but you have custom sRGB)
    finalColor.r = linear_rgb_to_srgb(finalColor.r);
    finalColor.g = linear_rgb_to_srgb(finalColor.g);
    finalColor.b = linear_rgb_to_srgb(finalColor.b);

    // ACES tone mapping
    finalColor = aces(finalColor);

    outColor[DTid.xy] = float4(finalColor, 1.0);
}