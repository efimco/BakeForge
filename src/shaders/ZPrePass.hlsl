cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
};

Texture2D albedoTexture : register(t0);
SamplerState samplerState : register(s0);

struct VertexInput
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
};


VertexOutput VS(VertexInput input)
{
	VertexOutput output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.texCoord = input.texCoord;
	return output;
}

// Pixel shader for alpha testing
void PS(VertexOutput input)
{
	uint2 albedoTexSize;
	albedoTexture.GetDimensions(albedoTexSize.x, albedoTexSize.y);
	bool hasAlbedoTexture = albedoTexSize.x > 0 && albedoTexSize.y > 0;
	float4 albedo = hasAlbedoTexture ? albedoTexture.Sample(samplerState, input.texCoord) : float4(1.0, 1.0, 1.0, 1.0);
	if (albedo.a < 0.5)
		discard;
}

