#include "primitive.hpp"

#include <fstream>
#include <iostream>

#include "assert.h"

using PrecomputedTri = bvh::v2::PrecomputedTri<Scalar>;
using StdOutputStream = bvh::v2::StdOutputStream;
using StdInputStream = bvh::v2::StdInputStream;

Primitive::Primitive(ComPtr<ID3D11Device> device, std::string_view nodeName)
	: SceneNode(nodeName)
	, m_sharedData(std::make_shared<SharedPrimitiveData>())
	, m_device(device)
{
}

void Primitive::setVertexData(std::vector<InterleavedData>&& vertexData) const
{

	m_sharedData->vertexData = std::move(vertexData);
	const auto numVerts = m_sharedData->vertexData.size();
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(numVerts * sizeof(InterleavedData));
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vertexSubresourceData;
	vertexSubresourceData.pSysMem = m_sharedData->vertexData.data();
	vertexSubresourceData.SysMemPitch = 0;
	vertexSubresourceData.SysMemSlicePitch = 0;
	HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &m_sharedData->vertexBuffer);
	assert(SUCCEEDED(hr));
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

const std::vector<uint32_t>& Primitive::getIndexData() const
{
	return m_sharedData->indexData;
}

const ComPtr<ID3D11Buffer>& Primitive::getIndexBuffer() const
{
	return m_sharedData->indexBuffer;
}

const ComPtr<ID3D11Buffer>& Primitive::getVertexBuffer() const
{
	return m_sharedData->vertexBuffer;
}

void Primitive::buildBVH() const
{
	if (m_sharedData->vertexData.empty())
	{
		return;
	}
	m_sharedData->triangles.reserve(m_sharedData->indexData.size() / 3);
	m_sharedData->bbox = std::make_unique<BBox>();

	for (size_t i = 0; i < m_sharedData->indexData.size(); i += 3)
	{
		if (i + 2 < m_sharedData->indexData.size())
		{
			Tri tri;
			tri.p0 = Vec3(m_sharedData->vertexData[m_sharedData->indexData[i]].position);
			tri.p1 = Vec3(m_sharedData->vertexData[m_sharedData->indexData[i + 1]].position);
			tri.p2 = Vec3(m_sharedData->vertexData[m_sharedData->indexData[i + 2]].position);
			m_sharedData->triangles.push_back(tri);

			m_sharedData->bbox->extend(tri.p0);
			m_sharedData->bbox->extend(tri.p1);
			m_sharedData->bbox->extend(tri.p2);
		}
	}
	m_sharedData->bboxes.resize(m_sharedData->triangles.size());
	m_sharedData->centers.resize(m_sharedData->triangles.size());
	for (size_t i = 0; i < m_sharedData->triangles.size(); ++i)
	{
		m_sharedData->bboxes[i] = m_sharedData->triangles[i].get_bbox();
		m_sharedData->centers[i] = m_sharedData->triangles[i].get_center();
	}
	m_sharedData->bvh = std::make_unique<Bvh>(
		bvh::v2::DefaultBuilder<Node>::build(m_sharedData->bboxes, m_sharedData->centers));
	std::printf("Built BVH with %zu nodes for %zu triangles.\n", m_sharedData->bvh->nodes.size()
	            , m_sharedData->triangles.size());
}

const Bvh* Primitive::getBVH() const
{
	return m_sharedData->bvh.get();
}

BBox Primitive::getWorldBBox(const glm::mat4& worldMatrix) const
{
	if (m_sharedData->vertexData.empty() || !m_sharedData->bbox)
	{
		return BBox::make_empty();
	}

	BBox worldBBox = BBox::make_empty();

	// Transform all 8 corners of the local AABB to get a conservative world AABB
	Vec3 localMin = m_sharedData->bbox->min;
	Vec3 localMax = m_sharedData->bbox->max;

	// All 8 corners of the local bbox
	glm::vec4 corners[8] = {glm::vec4(localMin[0], localMin[1], localMin[2], 1.0f)
	                        , glm::vec4(localMax[0], localMin[1], localMin[2], 1.0f)
	                        , glm::vec4(localMin[0], localMax[1], localMin[2], 1.0f)
	                        , glm::vec4(localMax[0], localMax[1], localMin[2], 1.0f)
	                        , glm::vec4(localMin[0], localMin[1], localMax[2], 1.0f)
	                        , glm::vec4(localMax[0], localMin[1], localMax[2], 1.0f)
	                        , glm::vec4(localMin[0], localMax[1], localMax[2], 1.0f)
	                        , glm::vec4(localMax[0], localMax[1], localMax[2], 1.0f)
	                        ,};

	for (auto corner : corners)
	{
		glm::vec4 worldCorner = worldMatrix * corner;
		worldBBox.extend(Vec3(worldCorner.x, worldCorner.y, worldCorner.z));
	}

	return worldBBox;
}

const BBox* Primitive::getLocalBBox() const
{
	return m_sharedData->bbox.get();
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

void Primitive::setSharedPrimitiveData(std::shared_ptr<SharedPrimitiveData> sharedData)
{
	m_sharedData = sharedData;
}
