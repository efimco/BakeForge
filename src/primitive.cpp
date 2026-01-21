#include "primitive.hpp"
#include <cstdint>
#include <d3d11.h>
#include <d3d11shader.h>

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

		HRESULT hr = m_device->CreateBuffer(&trisBufferDesc, &trisInitData, &m_sharedData->trisBuffer);
		assert(SUCCEEDED(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_sharedData->triangles.size());

		hr = m_device->CreateShaderResourceView(m_sharedData->trisBuffer.Get(), &srvDesc, &m_sharedData->srv_trisBuffer);
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
			m_device->CreateBuffer(&trisIndicesBufferDesc, &trisIndicesInitData, &m_sharedData->trisIndicesBuffer);
		assert(SUCCEEDED(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_sharedData->triangleIndices.size());

		hr = m_device->CreateShaderResourceView(m_sharedData->trisIndicesBuffer.Get(), &srvDesc,
			&m_sharedData->srv_trisIndicesBuffer);
		assert(SUCCEEDED(hr));
	}
}

void Primitive::buildBVH()
{
	BVH::BVHBuilder builder(m_sharedData->triangles, m_sharedData->triangleIndices);
	m_sharedData->bvhNodes = builder.BuildBVH();
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

ComPtr<ID3D11ShaderResourceView> Primitive::getTrisBufferSRV() const
{
	return m_sharedData->srv_trisBuffer;
}

ComPtr<ID3D11ShaderResourceView> Primitive::getTrisIndicesBufferSRV() const
{
	return m_sharedData->srv_trisIndicesBuffer;
}

const std::vector<Triangle>& Primitive::getTriangles() const
{
	return m_sharedData->triangles;
}

const std::vector<uint32_t>& Primitive::getTriangleIndices() const
{
	return m_sharedData->triangleIndices;
}

std::vector<BVH::Node>& Primitive::getBVHNodes() const
{
	return m_sharedData->bvhNodes;
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
