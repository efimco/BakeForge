

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

StructuredBuffer<Light> lights : register(t6); 
Texture2D<float4> tBackground : register(t7);
TextureCube tIrradianceMap : register(t8);
SamplerState linearSampler : register(s0);
RWTexture2D<unorm float4> outColor : register(u0); 


// Calculate IBL diffuse contribution
float3 calculateIBLDiffuse(float3 normal, float3 albedo, float metallic)
{
	// Sample irradiance cubemap
	float3 irradiance = tIrradianceMap.SampleLevel(linearSampler, normal, 0).rgb;
	
	// Apply non-metallic mask (metals don't have diffuse reflection)
	float3 diffuseContribution = albedo * irradiance * (1.0 - metallic);
	
	return diffuseContribution;
}

float3 applyPointLight(Light light, float3 fragPos, float3 normal, float3 albedo, float metallic, float roughness)
{
	float3 lightDir = normalize(light.position - fragPos);
	float3 viewDir = normalize(-fragPos);

	// Diffuse
	float diff = max(dot(normal, lightDir), 0.0);
	float3 diffuse = diff * light.color * albedo;

	// Specular
	float3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, halfwayDir), 0.0), 1.0 / roughness);
	float3 specular = spec * light.color * metallic;

	// Attenuation with radius
	float distance = length(light.position - fragPos);
	float attenuation = saturate(1.0 - distance / light.radius);
	if (distance < light.radius)
	{
		
		return (diffuse + specular) * light.intensity * attenuation;
	}
	return float3(0.0, 0.0, 0.0);
}

float3 applyDirectionalLight(Light light, float3 fragPos, float3 normal, float3 albedo, float metallic, float roughness)
{
	float3 lightDir = normalize(-light.direction);
	float3 viewDir = normalize(-fragPos);

	// Diffuse
	float diff = max(dot(normal, lightDir), 0.0);
	float3 diffuse = diff * light.color * albedo;

	// Specular
	float3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, halfwayDir), 0.0), 1.0 / roughness);
	float3 specular = spec * light.color * metallic;

	return (diffuse + specular) * light.intensity;
}

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{

	float3 fragPos = tFragPos.Load(int3(DTid.xy, 0)).xyz;
	float3 normal = tNormal.Load(int3(DTid.xy, 0)).xyz;
	float4 albedo = tAlbedo.Load(int3(DTid.xy, 0));
	float2 metallicRoughness = tMetallicRoughness.Load(int3(DTid.xy, 0)).xy;
	uint objectID = tObjectID.Load(int3(DTid.xy, 0));
	float depth = tDepth.Load(int3(DTid.xy, 0)).x;
	float3 background = tBackground.Load(int3(DTid.xy, 0)).xyz;
	
	float metallic = metallicRoughness.x;
	float roughness = max(metallicRoughness.y, 0.04); // Prevent division by zero
	
	float3 finalColor = float3(0.0, 0.0, 0.0);
	

	finalColor += calculateIBLDiffuse(normal, albedo.rgb, metallic);
	
	for (uint i = 0; i < lights.Length; ++i)
	{
		Light light = lights[i];
		if (light.lightType == 0) // Point light
		{
			finalColor += applyPointLight(light, fragPos, normal, albedo.rgb, metallic, roughness);
		}
		else if (light.lightType == 1) // Directional light
		{
			finalColor += applyDirectionalLight(light, fragPos, normal, albedo.rgb, metallic, roughness);
		}
	}

	finalColor = lerp(background, finalColor, albedo.a);
	
	finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
	
	outColor[DTid.xy] = float4(finalColor, 1.0);
}	