
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



static const float MAX_REFLECTION_LOD = 4.0;
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

uint querySpecularTextureLevels()
{
    uint width, height, levels;
    tPrefilteredMap.GetDimensions(0, width, height, levels);
    return levels;
}

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
    float3 fragPos = tFragPos.Load(int3(DTid.xy, 0)).xyz;
    float3 encodedNormal = tNormal.Load(int3(DTid.xy, 0)).xyz;
    float4 albedo = tAlbedo.Load(int3(DTid.xy, 0));
    float2 metallicRoughness = tMetallicRoughness.Load(int3(DTid.xy, 0)).xy;
    uint objectID = tObjectID.Load(int3(DTid.xy, 0));
    float depth = tDepth.Load(int3(DTid.xy, 0)).x;
    float3 background = tBackground.Load(int3(DTid.xy, 0)).xyz;

    float3 N = normalize(encodedNormal * 2.0 - 1.0);     // Decode normal from [0,1] to [-1,1]


    float metallic = metallicRoughness.x;
    float roughness = clamp(metallicRoughness.y, 0.04, 1.0); // Blender clamps roughness

    // Early exit for background
    if (objectID == 0)
    {
        outColor[DTid.xy] = float4(background, 1.0);
        return;
    }

    // View direction (camera space, so camera is at origin)
    float3 V = normalize(cameraPosition - fragPos);
    float NdotV = max(dot(N, V), 0.001);

    float3 R = reflect(-V, N);

    // Apply IBL rotation
    float3x3 irradianceRotation = getIrradianceMapRotation();
    float3 rotatedN = mul(irradianceRotation, N);
    float3 rotatedR = mul(irradianceRotation, R);

    // Calculate F0 (base reflectivity)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo.rgb, metallic);

    // IBL Diffuse
    float3 irradiance = tIrradianceMap.SampleLevel(linearSampler, rotatedN, 0).rgb;
    float3 F = fresnelSchlick(NdotV, F0, roughness);
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuseIBL = kD * albedo.rgb * irradiance;

    // IBL Specular
    float mipLevel = clamp(roughness * querySpecularTextureLevels()+1, 0.00, querySpecularTextureLevels());
    float3 prefilteredColor = tPrefilteredMap.SampleLevel(linearSampler, rotatedR, mipLevel).rgb;
    float2 brdf = tBRDFLUT.SampleLevel(linearSampler, float2(NdotV, roughness), 0).rg;
    float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

    // Combine IBL
    float3 finalColor = (diffuseIBL + specularIBL) * IBLintensity;
    // finalColor = prefilteredColor * IBLintensity;

    // // Add direct lighting
    // for (uint i = 0; i < lights.Length; ++i)
    // {
    //     Light light = lights[i];
    //     if (light.lightType == 0) // Point light
    //     {
    //         finalColor += applyPointLight(light, fragPos, N, V, albedo.rgb, metallic, roughness);
    //     }
    //     else if (light.lightType == 1) // Directional light
    //     {
    //         finalColor += applyDirectionalLight(light, N, V, albedo.rgb, metallic, roughness);
    //     }
    // }

    // Blender's ACES tonemapping (closer to Blender's Filmic)
    finalColor = aces(finalColor);

    // Gamma correction (Blender uses 2.2, but you have custom sRGB)
    finalColor.r = linear_rgb_to_srgb(finalColor.r);
    finalColor.g = linear_rgb_to_srgb(finalColor.g);
    finalColor.b = linear_rgb_to_srgb(finalColor.b);

    outColor[DTid.xy] = float4(finalColor, 1.0);
}