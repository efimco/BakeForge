
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
	float3 tangent : TANGENT0;
	float3 smoothedNormal : NORMAL1;
};

struct PSInput
{
	float4 position : SV_Position;
	float3 worldPos : WORLDPOS;
	float3 worldNormal : NORMAL;
	float3 worldTangent : TANGENT;
	float3 worldSmoothedNormal : NORMAL1;
};

struct PSOutput
{
	float4 worldPos : SV_TARGET0;
	float4 worldNormal : SV_TARGET1;
	float4 worldTangent : SV_TARGET2;
	float4 worldSmoothedNormal : SV_TARGET3;
};

PSInput VS(VSInput input)
{
	PSInput output;
	float2 clipPos = input.texCoord * 2.0f - 1.0f; // clip-space position [0,1] to [-1,1]
	clipPos.y = -clipPos.y;  
	output.position = float4(clipPos, 0.0f, 1.0f);
	

	output.worldPos = mul(float4(input.position, 1.0f), worldMatrix).xyz;
	output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldMatrixInvTranspose).xyz);
	output.worldTangent = normalize(mul(float4(input.tangent, 0.0f), worldMatrixInvTranspose).xyz);
	output.worldSmoothedNormal = normalize(mul(float4(input.smoothedNormal, 0.0f), worldMatrixInvTranspose).xyz);
	return output;
}

PSOutput PS(PSInput input)
{
	PSOutput output;
	output.worldPos = float4(input.worldPos, 1.0f);
	output.worldNormal = float4(normalize(input.worldNormal), 1.0f);
	output.worldTangent = float4(normalize(input.worldTangent), 1.0f);
	output.worldSmoothedNormal = float4(normalize(input.worldSmoothedNormal), 1.0f);
	return output;
}
