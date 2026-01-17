#include "constants.hlsl"


struct Vertex
{
	float3 position;
	float3 normal;
	float2 uv;
};

StructuredBuffer<Vertex> gVertices : register(t0);
StructuredBuffer<uint3> gIndices : register(t1);


cbuffer cb : register(b0)
{
	uint2 demensions;
	float2 padding;

	float3 camPosition;
	float padding2;

	float3 camForward;
	float padding3;

	float3 camRight;
	float padding4;

	float3 camUp;

	float camFOV;
}

struct Tri
{
	float3 v0, v1, v2;
	float3 normal;
};

float deg2Rad(float deg)
{
	float rad = deg * (PI / 180.0f);
	return rad;
}

StructuredBuffer<Tri> tris : register(t0);

struct Ray
{
	float3 origin;
	float3 dir;
};


// Möller–Trumbore intersection algorithm
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

[numthreads(16, 16, 1)]
void CS(uint2 tid : SV_DispatchThreadID)
{
	if (tid.x >= demensions.x || tid.y >= demensions.y)
	    return;

	float aspect = float(demensions.x) / float(demensions.y);

	float2 ndc = (float2(tid) + 0.5f) / float2(demensions);
    ndc = ndc * 2.0f - 1.0f;
    ndc.x *= aspect;

	float fov = deg2Rad(camFOV);

	float2 film = ndc * tan(fov * 0.5f);


	Ray ray;
	ray.origin = camPosition;

	//we start with camForward, then move right by film.x in the camRight direction and up by film.y in the camUp direction.
	ray.dir = normalize(camForward + film.x * camRight + film.y * camUp);


	float bestT = 1e30f;
    float3 bestN = 0.0f;

    uint triCount = gIndices.Length;

    // [loop]
	for (int i = 0; i < triCount; i++)
	{

		uint3 idx = gIndices[i];

		Tri tri;
		tri.v0 = gVertices[idx.x].position;
		tri.v1 = gVertices[idx.y].position;
		tri.v2 = gVertices[idx.z].position;

		tri.normal = normalize(cross(tri.v1 - tri.v0, tri.v2 - tri.v0));

		float t;
		if (IntersectTri(ray, tri, t))
		{
  			if (t < bestT)
            {
                bestT = t;
                bestN = tri.normal;
            }
		}
	}

	float3 color = float3(0,0,0);

 	if (bestT < 1e29f)
    {
        // simple visualization: normal -> color
        color = 0.5f * (bestN + 1.0f);

        // optional: distance fade
        // color *= 1.0f / (1.0f + 0.1f * bestT);
    }

	outputColor[tid] = float4(color, 1.0f);
}
