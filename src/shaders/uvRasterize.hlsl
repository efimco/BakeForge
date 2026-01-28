
cbuffer Constants : register(b0)
{
	float4x4 worldMatrix;
	float4x4 worldMatrixInvTranspose;
	uint2 dimensions;
	float cageOffset;
};

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD0;
};

struct PSInput
{
	float4 position : SV_Position;
	float3 worldPos : WORLDPOS;
	float3 worldNormal : NORMAL;
};

struct PSOutput
{
	float4 worldPos : SV_Target0;
	float4 worldNormal : SV_Target1;
};

PSInput VS(VSInput input)
{
	PSInput output;
	
	
	float2 clipPos = input.texCoord * 2.0f - 1.0f; // clip-space position [0,1] to [-1,1]
	clipPos.y = -clipPos.y;  
	output.position = float4(clipPos, 0.0f, 1.0f);
	

	output.worldPos = mul(float4(input.position, 1.0f), worldMatrix).xyz;
	output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldMatrixInvTranspose).xyz);
	
	return output;
}

PSOutput PS(PSInput input)
{
	PSOutput output;
	output.worldPos = float4(input.worldPos, 1.0f);
	output.worldNormal = float4(normalize(input.worldNormal), 1.0f);
	return output;
}
