#include "primitive.hpp"
#include "assert.h"
#include <fstream>
#include <iostream>

using PrecomputedTri = bvh::v2::PrecomputedTri<Scalar>;
using StdOutputStream = bvh::v2::StdOutputStream;
using StdInputStream = bvh::v2::StdInputStream;

Primitive::Primitive(ComPtr<ID3D11Device> device) : m_device(device) {}

void Primitive::setVertexData(std::vector<InterleavedData>&& vertexData)
{
	m_vertexData = std::move(vertexData);
	auto numVerts = m_vertexData.size();
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(numVerts * sizeof(InterleavedData));
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vertexSubresourceData;
	vertexSubresourceData.pSysMem = m_vertexData.data();
	vertexSubresourceData.SysMemPitch = 0;
	vertexSubresourceData.SysMemSlicePitch = 0;
	HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &m_vertexBuffer);
	assert(SUCCEEDED(hr));
}

void Primitive::setIndexData(std::vector<uint32_t>&& indexData)
{
	m_indexData = std::move(indexData);
	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = static_cast<UINT>(m_indexData.size() * sizeof(uint32_t));
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexInitData = {};
	indexInitData.pSysMem = m_indexData.data();
	HRESULT hr = m_device->CreateBuffer(&indexBufferDesc, &indexInitData, &m_indexBuffer);
	assert(SUCCEEDED(hr));

}

void Primitive::buildBVH()
{
	if (m_vertexData.empty())
	{
		return;
	}
	m_triangles.reserve(m_indexData.size() / 3);


	m_bbox = std::make_unique<BBox>();

	for (size_t i = 0; i < m_indexData.size(); i += 3)
	{
		if (i + 2 < m_indexData.size())
		{
			Tri tri;
			tri.p0 = Vec3(m_vertexData[m_indexData[i]].position);
			tri.p1 = Vec3(m_vertexData[m_indexData[i + 1]].position);
			tri.p2 = Vec3(m_vertexData[m_indexData[i + 2]].position);
			m_triangles.push_back(tri);

			m_bbox->extend(tri.p0);
			m_bbox->extend(tri.p1);
			m_bbox->extend(tri.p2);
		}
	}
	m_bboxes.resize(m_triangles.size());
	m_centers.resize(m_triangles.size());
	for (size_t i = 0; i < m_triangles.size(); ++i)
	{
		m_bboxes[i] = m_triangles[i].get_bbox();
		m_centers[i] = m_triangles[i].get_center();
	}
	m_bvh = std::make_unique<Bvh>(bvh::v2::DefaultBuilder<Node>::build(m_bboxes, m_centers));
	std::printf("Built BVH with %zu nodes for %zu triangles.\n", m_bvh->nodes.size(), m_triangles.size());
}

const Bvh* Primitive::getBVH() const
{
	return m_bvh.get();
}

BBox Primitive::getWorldBBox(glm::mat4 worldMatrix) const
{
	if (m_vertexData.empty() || !m_bbox)
	{
		return BBox::make_empty();
	}

	BBox worldBBox = BBox::make_empty();

	// Transform all 8 corners of the local AABB to get a conservative world AABB
	Vec3 localMin = m_bbox->min;
	Vec3 localMax = m_bbox->max;

	// All 8 corners of the local bbox
	glm::vec4 corners[8] = {
		glm::vec4(localMin[0], localMin[1], localMin[2], 1.0f),
		glm::vec4(localMax[0], localMin[1], localMin[2], 1.0f),
		glm::vec4(localMin[0], localMax[1], localMin[2], 1.0f),
		glm::vec4(localMax[0], localMax[1], localMin[2], 1.0f),
		glm::vec4(localMin[0], localMin[1], localMax[2], 1.0f),
		glm::vec4(localMax[0], localMin[1], localMax[2], 1.0f),
		glm::vec4(localMin[0], localMax[1], localMax[2], 1.0f),
		glm::vec4(localMax[0], localMax[1], localMax[2], 1.0f),
	};

	for (int i = 0; i < 8; ++i)
	{
		glm::vec4 worldCorner = worldMatrix * corners[i];
		worldBBox.extend(Vec3(worldCorner.x, worldCorner.y, worldCorner.z));
	}

	return worldBBox;
}

const BBox* Primitive::getLocalBBox() const
{
	return m_bbox.get();
}

std::unique_ptr<SceneNode> Primitive::clone()
{
	auto newPrimitive = std::make_unique<Primitive>(m_device);
	newPrimitive->name = this->name;
	newPrimitive->transform = this->transform;
	newPrimitive->visible = this->visible;
	newPrimitive->dirty = this->dirty;
	newPrimitive->movable = this->movable;
	if (!m_vertexData.empty())
	{
		std::vector<InterleavedData> vertexCopy = m_vertexData;
		newPrimitive->setVertexData(std::move(vertexCopy));
	}
	if (!m_indexData.empty())
	{
		std::vector<uint32_t> indexCopy = m_indexData;
		newPrimitive->setIndexData(std::move(indexCopy));
	}

	newPrimitive->material = this->material;
	newPrimitive->buildBVH();

	for (const auto& child : this->children)
	{
		std::unique_ptr<SceneNode> childClone = child->clone();
		newPrimitive->addChild(std::move(childClone));
	}
	
	return newPrimitive;
}


static bool save_bvh(const Bvh& bvh, const std::string& file_name) {
	std::ofstream out(file_name, std::ofstream::binary);
	if (!out)
		return false;
	StdOutputStream stream(out);
	bvh.serialize(stream);
	return true;
}

static std::optional<Bvh> load_bvh(const std::string& file_name) {
	std::ifstream in(file_name, std::ofstream::binary);
	if (!in)
		return std::nullopt;
	StdInputStream stream(in);
	return std::make_optional(Bvh::deserialize(stream));
}

