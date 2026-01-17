cbuffer cb : register(b0)
{
	uint2 demensions;
	float2 padding;
	float3 cameraPosition;
	float padding2;
}

struct Tri
{
	float3 v0, v1, v2;
	float3 normal;
	float3 center;
};

StructuredBuffer<Tri> tris : register(t0);

struct Ray
{
	float3 origin;
	float3 dir;
};

bool IntersectTri(Ray ray, Tri tri, out float t)
{
		t = 0.0f;
		float3 edge1 = tri.v1 - tri.v0;
		float3 edge2 = tri.v2 - tri.v0;
		float3 pvec = cross(ray.dir, edge2);
		float det = dot(edge1, pvec);

		if (det == 0.0f)
			return false;

		float invDet = 1.0f / det;
		float3 tvec = ray.origin - tri.v0;
		float u = dot(tvec, pvec) * invDet;
		if (u < 0.0f || u > 1.0f)
			return false;

		float3 qvec = cross(tvec, edge1);
		float v = dot(ray.dir, qvec) * invDet;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		t = dot(edge2, qvec) * invDet;
		if (t > 0.0f)
		{
			return true;
		}
		return false;
}

RWTexture2D<float4> outputColor : register(u0);

[numthreads(8, 8, 1)]
void CS(uint2 tid : SV_DispatchThreadID)
{
	float2 uv = (float2(tid) + 0.5f) / demensions;
	float3 color = float3(0.0f, 0.0f, 0.0f);

	Ray ray;
	ray.origin = cameraPosition;
	ray.dir = normalize(float3(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f, -1.0f));

	for (int i = 0; i < tris.Length; i++)
	{
		Tri tri = tris[i];
		float t;
		if (IntersectTri(ray, tri, t))
		{
			color += tri.normal * t;
		}
	}
	outputColor[tid] = float4(color, 1.0f);
}
