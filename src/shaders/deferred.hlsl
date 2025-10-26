

Texture2D tAlbedo : register(t0);
Texture2D tMetallicRoughness : register(t1);
Texture2D tNormal : register(t2);
Texture2D tFragPos : register(t3);
Texture2D tObjectID : register(t4);
Texture2D tDepth : register(t6);

struct Light
{
	int lightType;
	float3 position;
	float intensity;
	float3 color;
	float3 direction;
	float3 padding1; // Padding to align to 16 bytes
	float2 spotParams;
	float3 attenuations;
	float padding2; // Padding to align to 16 bytes
};

StructuredBuffer<Light> lights : register(t5);
RWTexture2D<float4> outColor : register(u0);

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	outColor[DTid.xy] = tAlbedo.Load(int3(DTid.xy, 0));
}