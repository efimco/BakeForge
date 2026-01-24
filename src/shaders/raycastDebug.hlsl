cbuffer CB : register(b0)
{
	matrix viewProjection;
	uint2 dimensions;
	float rayLength;
	uint padding;
};


Texture2D<float4> gWorldSpacePositions : register(t0);
Texture2D<float4> gWorldSpaceNormals : register(t1);

struct VSInput
{
	uint vertexID : SV_VertexID;
	uint instanceID : SV_InstanceID;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VS(VSInput input)
{
	PSInput output;

	uint texelIndex = input.instanceID;
	uint xCoord = texelIndex % dimensions.x;
	uint yCoord = uint(texelIndex / dimensions.x);
	uint2 texelCoord = uint2(xCoord, yCoord);

	float4 worldPos = gWorldSpacePositions.Load(int3(texelCoord, 0));
	float4 worldNormal = gWorldSpaceNormals.Load(int3(texelCoord, 0));


	if (worldPos.a == 0.0f) // we write alpha = 0 for invalid texels
	{
		output.position = float4(0, 0, 0, 0);
		output.color = float4(0, 0, 0, 0);
		return output;
	}

	float3 rayOrigin = worldPos.xyz;
	float3 rayDir = -normalize(worldNormal.xyz);
	rayOrigin -= rayDir * 0.015f; // offset to avoid self-intersection
	float3 rayEnd = rayOrigin + rayDir * rayLength;

	float3 vertexPos;
	if (input.vertexID == 0) // origin
	{
		vertexPos = rayOrigin;
		output.color = float4(0.0, 1.0, 0.0, 1.0); // Green 
	}
	else // direction
	{
		vertexPos = rayEnd;
		output.color = float4(1.0, 0.0, 0.0, 1.0); // Red
	}

	output.position = mul(float4(vertexPos, 1.0), viewProjection);
	return output;
}

float4 PS(PSInput input) : SV_TARGET
{
	return input.color;
}
