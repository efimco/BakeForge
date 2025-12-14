#include "objectPicker.hpp"
#include <iostream>
#include "inputEventsHandler.hpp"
#include "shaderManager.hpp"
#include "scene.hpp"
#include "primitive.hpp"

struct cbPicking
{
	uint32_t mousePosX;
	uint32_t mousePosY;
	uint32_t padding[2];
};

ObjectPicker::ObjectPicker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : m_device(device), m_context(context)
{
	m_shaderManager = std::make_unique<ShaderManager>(m_device);
	m_shaderManager->LoadComputeShader("colorPicker", L"../../src/shaders/colorPicker.hlsl", "CS");

	{ // constant buffer
		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = sizeof(cbPicking);
		constantBufferDesc.StructureByteStride = 0;
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
	}

	{ // gpu buffer
		D3D11_BUFFER_DESC gpuBufferDesc = {};
		gpuBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		gpuBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		gpuBufferDesc.ByteWidth = sizeof(readBackID);
		gpuBufferDesc.StructureByteStride = sizeof(readBackID);
		gpuBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		gpuBufferDesc.CPUAccessFlags = 0;
		HRESULT hr = m_device->CreateBuffer(&gpuBufferDesc, nullptr, &m_structuredBuffer);
		assert(SUCCEEDED(hr));
	}

	{// Staging buffer for readback
		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_STAGING;
		desc.ByteWidth = sizeof(readBackID);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.StructureByteStride = sizeof(readBackID);
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.BindFlags = 0;
		HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_stagingBuffer);
		assert(SUCCEEDED(hr));
	}

	{ //uav
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.Flags = 0;
		HRESULT hr = m_device->CreateUnorderedAccessView(m_structuredBuffer.Get(), &uavDesc, &m_uav);
	}
}

void ObjectPicker::dispatchPick(const ComPtr<ID3D11ShaderResourceView>& srv, uint32_t* mousePos, Scene* scene)
{
	{
		D3D11_MAPPED_SUBRESOURCE mappedCB = {};
		HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB);
		assert(SUCCEEDED(hr));
		cbPicking* cb = reinterpret_cast<cbPicking*>(mappedCB.pData);
		cb->mousePosX = mousePos[0];
		cb->mousePosY = mousePos[1];
		cb->padding[0] = 0;
		cb->padding[0] = 0;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	m_context->CSSetShader(m_shaderManager->getComputeShader("colorPicker"), nullptr, 0);
	m_context->CSSetShaderResources(0, 1, srv.GetAddressOf());
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, m_uav.GetAddressOf(), 0);

	m_context->Dispatch(1, 1, 1);


	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	m_context->CSSetShaderResources(0, 1, nullSRV);
	ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
	m_context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

	m_context->CopyResource(m_stagingBuffer.Get(), m_structuredBuffer.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	{
		HRESULT hr = m_context->Map(m_stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		assert(SUCCEEDED(hr));
		readBackID = *reinterpret_cast<uint32_t*>(mapped.pData);
		m_context->Unmap(m_stagingBuffer.Get(), 0);
	}
	if (InputEvents::isMouseClicked(MouseButtons::LEFT_BUTTON) && InputEvents::getMouseInViewport())
	{
		bool isShiftPressed = InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT);
		std::cout << "Picked ID: " << readBackID << std::endl;
		if (readBackID == 0)
		{
			scene->clearSelectedNodes();
		}
		else
		{
			scene->setActiveNode(scene->getPrimitiveByID(readBackID - 1), isShiftPressed);
		}
	}
}
