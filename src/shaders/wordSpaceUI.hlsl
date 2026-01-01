
Texture2D lightIcon : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewProjection;
	float4x4 view;
	float3 cameraPosition;
	float sizeInPixels;
	float2 screenSize;
	uint primitiveCount;
	float padding; 
};

struct Light
{
	int lightType;
	float3 position;
	float intensity;
	float3 color;
	float3 direction;
	float padding1; 
	float2 spotParams;
	float2 padding2;
	float3 attenuations;
	float radius; // radius for point lights
};


StructuredBuffer<Light> lights : register(t1);

struct VS_IN
{
	float3 position : POSITION;
	float2 uv : TEXCOORD0;
	uint iid : SV_INSTANCEID;
};

struct VS_OUT
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	uint iid : SV_INSTANCEID;
};

struct PS_OUT
{
	float4 color : SV_TARGET0;
	uint objectID : SV_TARGET1;
};


VS_OUT VS(VS_IN input)
{
	float3 lightPos = lights[input.iid].position;
	float4 clipPos = mul(float4(lightPos, 1.0f), viewProjection);
	float aspectRatio = screenSize.x / screenSize.y;
	float2 ndcPixelSize = sizeInPixels / screenSize * 2.0f;
	float2 offset = input.position.xy * ndcPixelSize * clipPos.w;

	VS_OUT output;
	output.position = clipPos + float4(offset, 0.0f, 0.0f);
	output.uv = input.uv;
	output.iid = input.iid;
	return output;
}

PS_OUT PS(VS_OUT input)
{
	PS_OUT output;
	float4 iconColor = lightIcon.Sample(samplerState, input.uv);
	output.objectID = primitiveCount + input.iid + 1; // Light IDs start after primitive IDs
	if (iconColor.a < 0.1f)
	{
		discard;
	}
	output.color = float4(lights[input.iid].color * iconColor.a, 1.0f);
	return output;
}
