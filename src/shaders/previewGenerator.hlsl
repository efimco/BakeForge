#include "BRDF.hlsl"
#include "constants.hlsl"

Texture2D albedoTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);

SamplerState samplerState : register(s0);

cbuffer cb : register(b0)
{
    float4x4 modelViewProjection;
    float4x4 inverseTransposedModel;
    float4x4 model;
}

static const float3 LIGHT_DIR = normalize(float3(-1.0f, 1.0f, 1.0f));
static const float3 LIGHT_COLOR = float3(1.0f, 1.0f, 1.0f);
static const float ambientFacotr = 0.1f;
static const float3 AMBIENT = float3(ambientFacotr, ambientFacotr, ambientFacotr);
static const float3 CAMERA_POS = float3(0.0f, 0.0f, 3.0f);
static const float LIGHT_INTENSITY = 10.0f;

struct VertexInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;
    float3 tangent : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
};

// Generate tangent from spherical UV mapping
float3 ComputeSphereTangent(float3 normal)
{
    // For a sphere, tangent points in the direction of increasing U (longitude)
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 tangent = cross(up, normal);

    // Handle poles where normal is parallel to up
    if (length(tangent) < 0.001f)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }

    return normalize(tangent);
}

VertexOutput VS(VertexInput input)
{
    VertexOutput output;

    output.position = mul(float4(input.position, 1.0f), modelViewProjection);
    output.texCoord = input.texCoord;

    // Transform normal to world space
    output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));

    // World position
    output.worldPos = mul(float4(input.position, 1.0f), model).xyz;

    // Compute tangent and bitangent for TBN matrix
    output.tangent = normalize(mul((float3x3)model, ComputeSphereTangent(input.normal)));
    output.bitangent = normalize(cross(output.normal, output.tangent));

    return output;
}

PSOutput PS(VertexOutput input)
{
    PSOutput output;
    // Sample textures
    float4 albedoSample = albedoTexture.Sample(samplerState, input.texCoord);
    if (albedoSample.a < 0.1)
    {
        discard;
    }
    float3 albedo = pow(albedoSample.rgb, 2.2f); // sRGB to linear

    float4 metallicRoughnessSample = metallicRoughnessTexture.Sample(samplerState, input.texCoord);
    float metallic = metallicRoughnessSample.b;   // Blue channel = metallic
    float roughness = metallicRoughnessSample.g;  // Green channel = roughness
    roughness = max(roughness, 0.04f); // Prevent division issues

    // Sample and transform normal map
    float3 normalSample = normalTexture.Sample(samplerState, input.texCoord).rgb;
    float3 tangentNormal = normalSample * 2.0f - 1.0f;

    // Build TBN matrix
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent);
    float3 B = normalize(input.bitangent);
    float3x3 TBN = float3x3(T, B, N);

    // Transform normal from tangent space to world space
    N = normalize(mul(tangentNormal, TBN));

    // View and light directions
    float3 V = normalize(CAMERA_POS - input.worldPos);
    float3 L = LIGHT_DIR;
    float3 H = normalize(V + L);

    // Dot products
    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float HdotV = max(dot(H, V), 0.0f);

    // Calculate reflectance at normal incidence
    // Dielectrics use 0.04, metals use albedo color
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    float3 F = fresnelSchlick(HdotV, F0, roughness);

    // Specular component
    float3 numerator = NDF * G * F;
    float denominator = 4.0f * NdotV * NdotL;
    float3 specular = numerator / max(denominator, 0.001f);

    // Energy conservation
    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0f - metallic; // Metals have no diffuse

    // Final lighting calculation
    float3 Lo = (kD * albedo / PI + specular) * LIGHT_COLOR * NdotL * LIGHT_INTENSITY;

    // Ambient lighting
    float3 ambient = AMBIENT * albedo;

    float3 color = ambient + Lo;

    // Tone mapping (Reinhard)
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // Gamma correction (linear to sRGB)
    color = pow(color, 1.0f / 2.2f);

    output.color = float4(color, albedoSample.a);
    return output;
}
