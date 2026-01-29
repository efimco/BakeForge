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
	float4 albedoColor;
	float metallicValue;
	float roughnessValue;
	uint hasAlbedoTexture;
	uint hasMetallicRoughnessTexture;
	uint hasNormalTexture;
}

static const float3 CAMERA_POS = float3(0.0f, 0.0f, 3.0f);

static const float3 LIGHT_POS = float3(3.0f, 4.0f, 4.0f);
static const float3 LIGHT_COLOR = float3(1.0f, 0.98f, 0.95f);
static const float LIGHT_RADIUS = 15.0f;
static const float LIGHT_INTENSITY = 800.0f;

static const float ambientFactor = 0.34f;
static const float3 AMBIENT = float3(ambientFactor, ambientFactor, ambientFactor);

// Secondary fill light (softer, from opposite side)
static const float3 FILL_LIGHT_POS = float3(-4.0f, 2.0f, -1.0f);
static const float3 FILL_LIGHT_COLOR = float3(0.6f, 0.7f, 0.9f);
static const float FILL_LIGHT_INTENSITY = 100.0f;

// Rim light (from behind)
static const float3 RIM_LIGHT_POS = float3(0.0f, 2.0f, -4.0f);
static const float3 RIM_LIGHT_COLOR = float3(1.0f, 1.0f, 1.0f);
static const float RIM_LIGHT_INTENSITY = 30.0f;

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


float3 ComputeSphereTangent(float3 normal)
{

	float3 up = float3(0.0f, 1.0f, 0.0f); 	// for a sphere tangent points in the direction of increasing U (longitude)
	float3 tangent = cross(up, normal);

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

	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));

	output.worldPos = mul(float4(input.position, 1.0f), model).xyz;

	output.tangent = normalize(mul((float3x3)model, ComputeSphereTangent(input.normal)));
	output.bitangent = normalize(cross(output.normal, output.tangent));

	return output;
}

PSOutput PS(VertexOutput input)
{
	PSOutput output;

	float4 albedoSample = float4(1.0f, 1.0f, 1.0f, 1.0f);
	if (hasAlbedoTexture == 1)
	{
		albedoSample = albedoTexture.Sample(samplerState, input.texCoord);
	}
	else
	{
		albedoSample = albedoColor;
	}

	if (albedoSample.a < 0.1)
	{
		discard;
	}
	float3 albedo = pow(albedoSample.rgb, 2.2f); // sRGB to linear

	float4 metallicRoughnessSample;
	if (hasMetallicRoughnessTexture == 1)
	{
		metallicRoughnessSample = metallicRoughnessTexture.Sample(samplerState, input.texCoord);
	}
	else
	{
		metallicRoughnessSample = float4(1.0f, roughnessValue, metallicValue, 1.0f);
	}
	float metallic = metallicRoughnessSample.b;
	float roughness = metallicRoughnessSample.g;
	roughness = max(roughness, 0.04f);

	float3 N = normalize(input.normal);
	float3 T = normalize(input.tangent);
	float3 B = normalize(input.bitangent);
	float3x3 TBN = float3x3(T, B, N);

	if (hasNormalTexture != 0)
	{
		float3 normalSample = normalTexture.Sample(samplerState, input.texCoord).rgb;
		float3 tangentNormal = normalSample * 2.0f - 1.0f;
		N = normalize(mul(tangentNormal, TBN));
	}

	float3 V = normalize(CAMERA_POS - input.worldPos);
	
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, metallic);
	
	// Slightly increase roughness for specular to make highlight larger in preview
	float specularRoughness = lerp(roughness, 1.0f, 0.2f);
	
	float NdotV = max(dot(N, V), 0.001f);
	
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	
	// Key Light 
	{
		float3 L = normalize(LIGHT_POS - input.worldPos);
		float3 H = normalize(V + L);
		
		float distance = length(LIGHT_POS - input.worldPos);
		float attenuation = saturate(pow(1.0 - pow(distance / LIGHT_RADIUS, 4.0), 2.0)) / (pow(distance, 2.0) + 1.0);
		float3 radiance = LIGHT_COLOR * LIGHT_INTENSITY * attenuation;
		
		float NdotL = max(dot(N, L), 0.0f);
		float NdotH = max(dot(N, H), 0.0f);
		float HdotV = max(dot(H, V), 0.0f);
		
		float NDF = distributionGGX(NdotH, specularRoughness);
		float G = geometrySmith(NdotV, NdotL, specularRoughness);
		float3 F = fresnelSchlick(HdotV, F0, roughness);
		
		float3 specular = (NDF * G * F) / max(4.0f * NdotV * NdotL, 0.001f);
		
		float3 kS = F;
		float3 kD = (1.0f - kS) * (1.0f - metallic);
		
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}
	
	// Fill Light 
	{
		float3 L = normalize(FILL_LIGHT_POS - input.worldPos);
		float3 H = normalize(V + L);
		
		float distance = length(FILL_LIGHT_POS - input.worldPos);
		float attenuation = 1.0f / (pow(distance, 2.0) + 1.0);
		float3 radiance = FILL_LIGHT_COLOR * FILL_LIGHT_INTENSITY * attenuation;
		
		float NdotL = max(dot(N, L), 0.0f);
		float NdotH = max(dot(N, H), 0.0f);
		float HdotV = max(dot(H, V), 0.0f);
		
		float NDF = distributionGGX(NdotH, specularRoughness);
		float G = geometrySmith(NdotV, NdotL, specularRoughness);
		float3 F = fresnelSchlick(HdotV, F0, roughness);
		
		float3 specular = (NDF * G * F) / max(4.0f * NdotV * NdotL, 0.001f);
		
		float3 kS = F;
		float3 kD = (1.0f - kS) * (1.0f - metallic);
		
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}
	
	// Rim Light
	{
		float3 L = normalize(RIM_LIGHT_POS - input.worldPos);
		float3 H = normalize(V + L);
		
		float distance = length(RIM_LIGHT_POS - input.worldPos);
		float attenuation = 1.0f / (pow(distance, 2.0) + 1.0);
		float3 radiance = RIM_LIGHT_COLOR * RIM_LIGHT_INTENSITY * attenuation;
		
		float NdotL = max(dot(N, L), 0.0f);
		float NdotH = max(dot(N, H), 0.0f);
		float HdotV = max(dot(H, V), 0.0f);
		
		// Fresnel rim effect - stronger at grazing angles
		float rim = pow(1.0f - NdotV, 3.0f);
		
		float NDF = distributionGGX(NdotH, specularRoughness);
		float G = geometrySmith(NdotV, NdotL, specularRoughness);
		float3 F = fresnelSchlick(HdotV, F0, roughness);
		
		float3 specular = (NDF * G * F) / max(4.0f * NdotV * NdotL, 0.001f);
		
		float3 kS = F;
		float3 kD = (1.0f - kS) * (1.0f - metallic);
		
		Lo += ((kD * albedo / PI + specular) * radiance * NdotL) + (rim * radiance * 0.15f);
	}

	float3 ambient = AMBIENT * albedo;
	float3 color = ambient + Lo;
	
	// HDR tonemapping (Reinhard)
	color = color / (color + float3(1.0f, 1.0f, 1.0f));

	output.color = float4(color, 1.0f);
	return output;
}
