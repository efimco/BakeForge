struct Tri
{
	float3 v0;
	float pad0;
	float3 v1;
	float pad1;
	float3 v2;
	float pad2;
	float3 n0;  // vertex normal at v0
	float pad3;
	float3 n1;  // vertex normal at v1
	float pad4;
	float3 n2;  // vertex normal at v2
	float pad5;
};

struct BBox
{
	float3 min;
	float pad0;
	float3 max;
	float pad1;
};

struct BVHNode
{
	BBox bbox;
	uint leftChild, firstTriIndex, numTris;
	uint depth;
};

struct Ray
{
	float3 origin;
	float3 dir;
	float3 invDir;
};

// Möller–Trumbore intersection algorithm - outputs barycentric coords for normal interpolation
bool IntersectTri(Ray ray, Tri tri, inout float t, out float2 baryOut)
{
	baryOut = float2(0, 0);
	float3 edge1 = tri.v1 - tri.v0;
	float3 edge2 = tri.v2 - tri.v0;
	float3 pvec = cross(ray.dir, edge2);
	float det = dot(edge1, pvec);


	if (abs(det) < 1e-8f) 	// small epsilon for near-parallel rays
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

	float hitT = dot(edge2, qvec) * invDet;
	if (hitT > 0.0f && hitT < t)
	{
		t = hitT;
		baryOut = float2(u, v);
		return true;
	}
	return false;
}

// Optimized slab method - returns tNear for ordering, uses precomputed invDir
float IntersectBox(Ray ray, BBox box, float tMax)
{
	float3 t0 = (box.min - ray.origin) * ray.invDir;
	float3 t1 = (box.max - ray.origin) * ray.invDir;

	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);

	float tNear = max(max(tmin.x, tmin.y), tmin.z);
	float tFar = min(min(tmax.x, tmax.y), tmax.z);

	// Return tNear if hit, otherwise return large value
	return (tNear <= tFar && tFar >= 0.0f && tNear < tMax) ? tNear : 1e30f;
}

bool IsLeaf(BVHNode node)
{
	return node.numTris != 0;
}
