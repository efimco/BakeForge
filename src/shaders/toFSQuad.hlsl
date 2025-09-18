// toFSQuad.hlsl

Texture2D FSQuadTexture : register(t0);

SamplerState samplerState : register(s0);

struct VertexInput
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

struct PSOutput
{
	float4 finalTexture : SV_TARGET0;
};

VertexOutput VS(VertexInput input)
{
	VertexOutput output;
	output.position = float4(input.position, 1.0f);
	output.texCoord = input.texCoord;
	return output;
}

PSOutput PS(VertexOutput input)
{
	PSOutput output;
	output.finalTexture = FSQuadTexture.Sample(samplerState, input.texCoord);
	return output;
}
