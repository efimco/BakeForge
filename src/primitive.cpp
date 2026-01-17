#include "primitive.hpp"
#include <d3d11.h>
#include <d3d11shader.h>

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

	D3D11_BUFFER_DESC vertexStructuredBufferDesc = {};
	vertexStructuredBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexStructuredBufferDesc.ByteWidth = static_cast<UINT>(numVerts * sizeof(Vertex));
	vertexStructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	vertexStructuredBufferDesc.CPUAccessFlags = 0;
	vertexStructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	vertexStructuredBufferDesc.StructureByteStride = sizeof(Vertex);

	D3D11_SHADER_RESOURCE_VIEW_DESC vertexStructuredBufferSRVDesc = {};
	vertexStructuredBufferSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexStructuredBufferSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	vertexStructuredBufferSRVDesc.Buffer.FirstElement = 0;
	vertexStructuredBufferSRVDesc.Buffer.NumElements = static_cast<UINT>(numVerts);


	hr = m_device->CreateBuffer(&vertexStructuredBufferDesc, &vertexSubresourceData, &m_sharedData->vertexStructuredBuffer);
	assert(SUCCEEDED(hr));

	hr = m_device->CreateShaderResourceView(m_sharedData->vertexStructuredBuffer.Get(), &vertexStructuredBufferSRVDesc, &m_sharedData->srv_vertexStructuredBuffer);
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


	std::vector<glm::uvec3> indexStructuredData;

	for (size_t i = 0; i < m_sharedData->indexData.size(); i += 3)
	{
		indexStructuredData.push_back(glm::uvec3(m_sharedData->indexData[i],
												 m_sharedData->indexData[i + 1],
												 m_sharedData->indexData[i + 2]));
	}

	D3D11_BUFFER_DESC indexStructuredBufferDesc = {};
	indexStructuredBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexStructuredBufferDesc.ByteWidth = static_cast<UINT>(indexStructuredData.size() * sizeof(glm::uvec3));
	indexStructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	indexStructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	indexStructuredBufferDesc.StructureByteStride = sizeof(glm::uvec3);
	indexStructuredBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA indexStructuredInitData = {};
	indexStructuredInitData.pSysMem = indexStructuredData.data();

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(indexStructuredData.size());

	hr = m_device->CreateBuffer(&indexStructuredBufferDesc, &indexStructuredInitData, &m_sharedData->indexStructuredBuffer);
	assert(SUCCEEDED(hr));

	hr = m_device->CreateShaderResourceView(m_sharedData->indexStructuredBuffer.Get(), &srvDesc, &m_sharedData->srv_indexStructuredBuffer);
	assert(SUCCEEDED(hr));
}

const std::vector<uint32_t>& Primitive::getIndexData() const
{
	return m_sharedData->indexData;
}

ComPtr<ID3D11Buffer> Primitive::getIndexBuffer() const
{
	return m_sharedData->indexBuffer;
}

ComPtr<ID3D11Buffer> Primitive::getVertexBuffer() const
{
	return m_sharedData->vertexBuffer;
}

ComPtr<ID3D11ShaderResourceView> Primitive::getVertexStructuredBufferSRV() const
{
	return m_sharedData->srv_vertexStructuredBuffer;
}

ComPtr<ID3D11ShaderResourceView> Primitive::getIndexStructuredBufferSRV() const
{
	return m_sharedData->srv_indexStructuredBuffer;
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
