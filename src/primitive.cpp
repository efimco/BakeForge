#include "primitive.hpp"

#include "assert.h"
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
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(numVerts * sizeof(Vertex));
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

void Primitive::fillTriangles()
{

	for (size_t i = 0; i < m_sharedData->indexData.size(); i += 3)
	{
		uint32_t& i0 = m_sharedData->indexData[i];
		uint32_t& i1 = m_sharedData->indexData[i + 1];
		uint32_t& i2 = m_sharedData->indexData[i + 2];

		Vertex& v0 = m_sharedData->vertexData[i0];
		Vertex& v1 = m_sharedData->vertexData[i1];
		Vertex& v2 = m_sharedData->vertexData[i2];

		Triangle tri(v0.position, v1.position, v2.position);
		tri.center = (glm::vec3(v0.position) + glm::vec3(v1.position) + glm::vec3(v2.position)) / 3.0f;
		tri.normal = glm::normalize(glm::vec3(v0.normal.x, v0.normal.y, v0.normal.z) +
									glm::vec3(v1.normal.x, v1.normal.y, v1.normal.z) +
									glm::vec3(v2.normal.x, v2.normal.y, v2.normal.z));
		m_sharedData->triangles.push_back(tri);
	}

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = static_cast<UINT>(sizeof(Triangle) * m_sharedData->triangles.size());
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(Triangle);

	D3D11_SUBRESOURCE_DATA subresData = {};
	subresData.pSysMem = m_sharedData->triangles.data();
	HRESULT hr = m_device->CreateBuffer(&desc, &subresData, &m_sharedData->structuredBuffer);
	assert(SUCCEEDED(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<uint32_t>(m_sharedData->triangles.size());
	hr = m_device->CreateShaderResourceView(m_sharedData->structuredBuffer.Get(), &srvDesc, &m_sharedData->srv_structuredBuffer);
	assert(SUCCEEDED(hr));
}

ComPtr<ID3D11ShaderResourceView> Primitive::getTrianglesStructuredBufferSRV()
{
	return m_sharedData->srv_structuredBuffer;
}
