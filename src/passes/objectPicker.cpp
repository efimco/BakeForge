#include "objectPicker.hpp"

#include <intsafe.h>
#include <iostream>

#include "basePass.hpp"
#include "inputEventsHandler.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

struct cbPicking
{
	uint32_t mousePosX;
	uint32_t mousePosY;
	uint32_t padding[2];
};

ObjectPicker::ObjectPicker(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context), readBackID(0)
{
	m_device = device;
	m_context = context;

	m_shaderManager->LoadComputeShader("colorPicker", L"../../src/shaders/colorPicker.hlsl", "CS");

	m_constantBuffer = createConstantBuffer(sizeof(cbPicking));

	m_structuredBuffer = createStructuredBuffer(sizeof(readBackID), 1, SBPreset::Default);
	m_stagingBuffer = createStructuredBuffer(sizeof(readBackID), 1, SBPreset::CpuRead);
	m_uav = createUnorderedAccessView(m_structuredBuffer.Get(), UAVPreset::StructuredBuffer, 0, 1);
}

void ObjectPicker::dispatchPick(const ComPtr<ID3D11ShaderResourceView>& srv, const uint32_t* mousePos, Scene* scene)
{
	if (InputEvents::isMouseClicked(MouseButtons::LEFT_BUTTON) && InputEvents::getMouseInViewport())
	{
		{
			D3D11_MAPPED_SUBRESOURCE mappedCB = {};
			HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB);
			assert(SUCCEEDED(hr));
			const auto cb = static_cast<cbPicking*>(mappedCB.pData);
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

		unbindComputeUAVs(0, 1);
		unbindShaderResources(0, 1);

		m_context->CopyResource(m_stagingBuffer.Get(), m_structuredBuffer.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		{
			HRESULT hr = m_context->Map(m_stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
			assert(SUCCEEDED(hr));
			readBackID = *static_cast<float*>(mapped.pData);
			m_context->Unmap(m_stagingBuffer.Get(), 0);
		}

		std::cout << "Picked ID: " << readBackID << std::endl;
		scene->setReadBackID(readBackID);
	}
}
