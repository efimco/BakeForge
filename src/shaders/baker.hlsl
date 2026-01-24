
cbuffer Constants : register(b0)
{
	float4x4 worldMatrix;
	float4x4 worldMatrixInvTranspose;
	uint2 dimensions;
};

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texCoords;
};

struct Tri
{
	Vertex v0;
	Vertex v1;
	Vertex v2;
};

StructuredBuffer<Vertex> gVerticies : register(t0);
StructuredBuffer<uint> gIndicies : register(t1);

RWTexture2D<float4> oWorldSpaceTexelPosition : register(u0);
RWTexture2D<float4> oWorldSpaceTexelNormal : register(u1);

bool uvInsideTri(float2 uv, Tri tri, out float3 barycentric)
{
	float2 v0 = tri.v1.texCoords - tri.v0.texCoords;
	float2 v1 = tri.v2.texCoords - tri.v0.texCoords;
	float2 v2 = uv - tri.v0.texCoords;

	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;

	barycentric.y = (d11 * d20 - d01 * d21) / denom;
	barycentric.z = (d00 * d21 - d01 * d20) / denom;
	barycentric.x = 1.0f - barycentric.y - barycentric.z;

	return (barycentric.x >= 0) && (barycentric.y >= 0) && (barycentric.z >= 0);
}


[numthreads(32, 32, 1)]
void CS(uint3 DTid : SV_DispatchThreadID)
{
	float2 uv = (float2(DTid.xy) + 0.5f) / dimensions;

	for (uint i = 0; i < gIndicies.Length; i += 3)
	{
		Tri tri;
		tri.v0 = gVerticies[gIndicies[i + 0]];
		tri.v1 = gVerticies[gIndicies[i + 1]];
		tri.v2 = gVerticies[gIndicies[i + 2]];
		float3 barycentric;
		if (uvInsideTri(uv, tri, barycentric))
		{
			float3 localPos = barycentric.x * tri.v0.position
				+ barycentric.y * tri.v1.position
				+ barycentric.z * tri.v2.position;

			float3 localNormal = normalize(
				barycentric.x * tri.v0.normal
				+ barycentric.y * tri.v1.normal
				+ barycentric.z * tri.v2.normal);

			float3 worldPos = mul(float4(localPos, 1.0f), worldMatrix).xyz;
			float3 worldNormal = mul(float4(localNormal, 1.0f), worldMatrixInvTranspose).xyz;
			oWorldSpaceTexelPosition[DTid.xy] = float4(worldPos, 1.0f);
			oWorldSpaceTexelNormal[DTid.xy] = float4(worldNormal, 1.0f);
			break;
		}
		oWorldSpaceTexelPosition[DTid.xy] = float4(0.0, 0.0, 0.0f, 0.0f);
		oWorldSpaceTexelNormal[DTid.xy] = float4(0.0, 0.0, 0.0f, 0.0f);
	}
}