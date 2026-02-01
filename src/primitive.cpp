#include "primitive.hpp"
#include <cstdint>
#include <d3d11.h>
#include <d3d11shader.h>
#include <iostream>

#include "assert.h"

#include "bvhBuilder.hpp"
#include "primitiveData.hpp"

Primitive::Primitive(ComPtr<ID3D11Device> device, std::string_view nodeName)
	: SceneNode(nodeName)
	, m_sharedData(std::make_shared<SharedPrimitiveData>())
	, m_device(device)
{
}

void Primitive::setVertexData(std::vector<Vertex>&& vertexData) const
{

	m_sharedData->vertexData = std::move(vertexData);
	const auto numVerts = m_sharedData->vertexData.size();
}

void Primitive::setIndexData(std::vector<uint32_t>&& indexData) const
{
	m_sharedData->indexData = std::move(indexData);

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = static_cast<UINT>(m_sharedData->indexData.size() * sizeof(uint32_t));
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexInitData = {};
	indexInitData.pSysMem = m_sharedData->indexData.data();
	HRESULT hr = m_device->CreateBuffer(&indexBufferDesc, &indexInitData, &m_sharedData->indexBuffer);
	assert(SUCCEEDED(hr));
}

void Primitive::fillTriangles()
{
	for (size_t i = 0; i < m_sharedData->indexData.size(); i += 3)
	{
		Triangle tri;
		tri.v0 = m_sharedData->vertexData[m_sharedData->indexData[i]].position;
		tri.v1 = m_sharedData->vertexData[m_sharedData->indexData[i + 1]].position;
		tri.v2 = m_sharedData->vertexData[m_sharedData->indexData[i + 2]].position;
		tri.n0 = m_sharedData->vertexData[m_sharedData->indexData[i]].normal;
		tri.n1 = m_sharedData->vertexData[m_sharedData->indexData[i + 1]].normal;
		tri.n2 = m_sharedData->vertexData[m_sharedData->indexData[i + 2]].normal;
		m_sharedData->triangles.push_back(tri);
	}

	for (uint32_t i = 0; i < m_sharedData->triangles.size(); i++)
	{
		m_sharedData->triangleIndices.push_back(i);
	}

	buildBVH(); // Build BVH BEFORE creating GPU buffers - BVH builder reorders triangleIndices

	{
		D3D11_BUFFER_DESC trisBufferDesc = {};
		trisBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		trisBufferDesc.ByteWidth = static_cast<UINT>(m_sharedData->triangles.size() * sizeof(Triangle));
		trisBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		trisBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		trisBufferDesc.StructureByteStride = sizeof(Triangle);

		D3D11_SUBRESOURCE_DATA trisInitData = {};
		trisInitData.pSysMem = m_sharedData->triangles.data();

		HRESULT hr = m_device->CreateBuffer(&trisBufferDesc, &trisInitData, &m_sharedData->structuredTrisBuffer);
		assert(SUCCEEDED(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_sharedData->triangles.size());

		hr = m_device->CreateShaderResourceView(m_sharedData->structuredTrisBuffer.Get(), &srvDesc, &m_sharedData->srv_structuredTrisBuffer);
		assert(SUCCEEDED(hr));
	}

	{
		D3D11_BUFFER_DESC trisIndicesBufferDesc = {};
		trisIndicesBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		trisIndicesBufferDesc.ByteWidth = static_cast<UINT>(m_sharedData->triangleIndices.size() * sizeof(uint32_t));
		trisIndicesBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		trisIndicesBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		trisIndicesBufferDesc.StructureByteStride = sizeof(uint32_t);

		D3D11_SUBRESOURCE_DATA trisIndicesInitData = {};
		trisIndicesInitData.pSysMem = m_sharedData->triangleIndices.data();

		HRESULT hr =
			m_device->CreateBuffer(&trisIndicesBufferDesc, &trisIndicesInitData, &m_sharedData->structuredTrisIndicesBuffer);
		assert(SUCCEEDED(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_sharedData->triangleIndices.size());

		hr = m_device->CreateShaderResourceView(m_sharedData->structuredTrisIndicesBuffer.Get(), &srvDesc,
			&m_sharedData->srv_structuredTrisIndicesBuffer);
		assert(SUCCEEDED(hr));
	}
	computeTangents();
	computeSmoothNormals();
	createGPUBuffers();
}

BVH::BBox Primitive::getWorldBBox()
{
	BVH::BBox bbox = m_sharedData->bvhNodes[0].bbox;
	for (int i = 0; i < 3; ++i)
	{
		bbox.min[i] += transform.position[i];
		bbox.max[i] += transform.position[i];
	}
	return bbox;
}

void Primitive::computeTangents()
{
	const auto& indices = getIndexData();

	for (auto& v : m_sharedData->vertexData)
	{
		v.tangent = glm::vec3(0.0f);
	}

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		uint32_t i0 = indices[i];
		uint32_t i1 = indices[i + 1];
		uint32_t i2 = indices[i + 2];

		const glm::vec3& p0 = m_sharedData->vertexData[i0].position;
		const glm::vec3& p1 = m_sharedData->vertexData[i1].position;
		const glm::vec3& p2 = m_sharedData->vertexData[i2].position;

		const glm::vec2& uv0 = m_sharedData->vertexData[i0].texCoords;
		const glm::vec2& uv1 = m_sharedData->vertexData[i1].texCoords;
		const glm::vec2& uv2 = m_sharedData->vertexData[i2].texCoords;

		glm::vec3 edge1 = p1 - p0;
		glm::vec3 edge2 = p2 - p0;
		glm::vec2 deltaUV1 = uv1 - uv0;
		glm::vec2 deltaUV2 = uv2 - uv0;

		float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y + 1e-8f);

		glm::vec3 tangent;
		tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

		m_sharedData->vertexData[i0].tangent += tangent;
		m_sharedData->vertexData[i1].tangent += tangent;
		m_sharedData->vertexData[i2].tangent += tangent;
	}

	// Normalize and orthogonalize
	for (auto& v : m_sharedData->vertexData)
	{
		v.tangent = glm::normalize(v.tangent);
		// Gram-Schmidt orthogonalize
		v.tangent = glm::normalize(v.tangent - v.normal * glm::dot(v.normal, v.tangent));
	}
}

void Primitive::computeSmoothNormals()
{
	// Map from position to accumulated normal
	std::unordered_map<size_t, glm::vec3> positionToNormal;

	auto hashPos = [](const glm::vec3& p) -> size_t {
		// Quantize to avoid floating point issues
		int x = static_cast<int>(p.x * 10000.0f);
		int y = static_cast<int>(p.y * 10000.0f);
		int z = static_cast<int>(p.z * 10000.0f);
		return std::hash<int>()(x) ^ (std::hash<int>()(y) << 1) ^ (std::hash<int>()(z) << 2);
		};

	// Accumulate normals by position
	for (const auto& v : m_sharedData->vertexData)
	{
		size_t key = hashPos(v.position);
		positionToNormal[key] += v.normal;
	}

	// Normalize accumulated normals
	for (auto& [key, normal] : positionToNormal)
	{
		normal = glm::normalize(normal);
	}

	// Assign smoothed normals back to vertices
	for (auto& v : m_sharedData->vertexData)
	{
		size_t key = hashPos(v.position);
		v.smoothNormal = positionToNormal[key];
	}
}


void Primitive::buildBVH()
{
	BVH::BVHBuilder builder(m_sharedData->triangles, m_sharedData->triangleIndices);
	m_sharedData->bvhNodes = builder.BuildBVH();

	// auto stats = BVH::BVHBuilder::CalculateStats(m_sharedData->bvhNodes);
	// std::cout << "BVH for: " << name << std::endl;
	// BVH::BVHBuilder::PrintStats(stats);
}

void Primitive::createGPUBuffers()
{
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(m_sharedData->vertexData.size() * sizeof(Vertex));
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexSubresourceData = {};
	vertexSubresourceData.pSysMem = m_sharedData->vertexData.data();

	m_sharedData->vertexBuffer.Reset();
	HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &m_sharedData->vertexBuffer);
	assert(SUCCEEDED(hr));
}


const std::vector<uint32_t>& Primitive::getIndexData() const
{
	return m_sharedData->indexData;
}

const std::vector<Vertex>& Primitive::getVertexData() const
{
	return m_sharedData->vertexData;
}

ComPtr<ID3D11Buffer> Primitive::getIndexBuffer() const
{
	return m_sharedData->indexBuffer;
}

ComPtr<ID3D11Buffer> Primitive::getVertexBuffer() const
{
	return m_sharedData->vertexBuffer;
}

ComPtr<ID3D11ShaderResourceView> Primitive::getTrisBufferSRV() const
{
	return m_sharedData->srv_structuredTrisBuffer;
}

ComPtr<ID3D11ShaderResourceView> Primitive::getTrisIndicesBufferSRV() const
{
	return m_sharedData->srv_structuredTrisIndicesBuffer;
}

const std::vector<Triangle>& Primitive::getTriangles() const
{
	return m_sharedData->triangles;
}

std::vector<Triangle> Primitive::getWorldSpaceTriangles()
{
	std::vector<Triangle> worldTris;
	worldTris.reserve(m_sharedData->triangles.size());

	glm::mat4 worldMatrix = getWorldMatrix();

	for (const Triangle& tri : m_sharedData->triangles)
	{
		Triangle worldTri;
		worldTri.v0 = glm::vec3(worldMatrix * glm::vec4(tri.v0, 1.0f));
		worldTri.v1 = glm::vec3(worldMatrix * glm::vec4(tri.v1, 1.0f));
		worldTri.v2 = glm::vec3(worldMatrix * glm::vec4(tri.v2, 1.0f));

		// Transform normals (ignore translation)
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
		worldTri.n0 = glm::normalize(normalMatrix * tri.n0);
		worldTri.n1 = glm::normalize(normalMatrix * tri.n1);
		worldTri.n2 = glm::normalize(normalMatrix * tri.n2);

		worldTris.push_back(worldTri);
	}

	return worldTris;
}

const std::vector<uint32_t>& Primitive::getTriangleIndices() const
{
	return m_sharedData->triangleIndices;
}

std::vector<BVH::Node>& Primitive::getBVHNodes() const
{
	return m_sharedData->bvhNodes;
}
std::vector<BVH::Node> Primitive::getWorldSpaceBVHNodes()
{
	std::vector<BVH::Node> worldNodes;
	worldNodes.reserve(m_sharedData->bvhNodes.size());

	glm::mat4 worldMatrix = getWorldMatrix();

	for (const BVH::Node& node : m_sharedData->bvhNodes)
	{
		BVH::Node worldNode = node;

		// Transform bbox corners to world space and recompute AABB
		glm::vec3 localMin = node.bbox.min;
		glm::vec3 localMax = node.bbox.max;

		glm::vec3 corners[8] = {
			{localMin.x, localMin.y, localMin.z},
			{localMax.x, localMin.y, localMin.z},
			{localMin.x, localMax.y, localMin.z},
			{localMax.x, localMax.y, localMin.z},
			{localMin.x, localMin.y, localMax.z},
			{localMax.x, localMin.y, localMax.z},
			{localMin.x, localMax.y, localMax.z},
			{localMax.x, localMax.y, localMax.z},
		};

		glm::vec3 worldMin(FLT_MAX);
		glm::vec3 worldMax(-FLT_MAX);

		for (int c = 0; c < 8; c++)
		{
			glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corners[c], 1.0f));
			worldMin = glm::min(worldMin, worldCorner);
			worldMax = glm::max(worldMax, worldCorner);
		}

		worldNode.bbox.min = worldMin;
		worldNode.bbox.max = worldMax;

		worldNodes.push_back(worldNode);
	}

	return worldNodes;
}

void Primitive::copyFrom(const SceneNode& node)
{
	name = node.name;
	transform = node.transform;

	if (const auto primitiveNode = dynamic_cast<const Primitive*>(&node))
	{
		m_sharedData = primitiveNode->m_sharedData;
		material = primitiveNode->material;
	}
}

bool Primitive::differsFrom(const SceneNode& node) const
{
	if (!SceneNode::differsFrom(node))
	{
		if (const auto primitiveNode = dynamic_cast<const Primitive*>(&node))
		{
			return material != primitiveNode->material;
		}
	}
	return true;
}

std::unique_ptr<SceneNode> Primitive::clone() const
{
	std::unique_ptr<Primitive> primitive = std::make_unique<Primitive>(m_device);
	primitive->copyFrom(*this);
	return primitive;
}
