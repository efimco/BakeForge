

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
RWTexture2D<unorm float4> outColor : register(u0); 

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	outColor[DTid.xy] = tAlbedo.Load(int3(DTid.xy, 0));
	float3 fragPos = tFragPos.Load(int3(DTid.xy, 0)).xyz;
	float3 normal = tNormal.Load(int3(DTid.xy, 0)).xyz;
	float3 albedo = tAlbedo.Load(int3(DTid.xy, 0)).xyz;
	float metallic = tMetallicRoughness.Load(int3(DTid.xy, 0)).x;
	float roughness = tMetallicRoughness.Load(int3(DTid.xy, 0)).y;
	uint objectID = tObjectID.Load(int3(DTid.xy, 0)); 
	float depth = tDepth.Load(int3(DTid.xy, 0)).x;

	float3 finalColor = float3(0.0, 0.0, 0.0);

	for (uint i = 0; i < lights.Length; ++i)
	{
		Light light = lights[i];
		if (light.lightType == 0) // Point light
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
				
				finalColor = 0;
			}
			finalColor += (diffuse + specular) * light.intensity * attenuation;
		}
		else if (light.lightType == 1) // Directional light
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

			finalColor += (diffuse + specular) * light.intensity;
		}
	}

	// Visualize objectID for debugging (convert uint to normalized float)
	float idValue = float(objectID) / 255.0;
	outColor[DTid.xy] = float4(finalColor, 1.0);
}	