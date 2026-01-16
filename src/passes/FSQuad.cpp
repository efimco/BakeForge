#include "FSQuad.hpp"

#include <cassert>
#include <iostream>

#include "appConfig.hpp"
#include "basePass.hpp"
#include "rtvCollector.hpp"
#include "shaderManager.hpp"

FSQuad::FSQuad(ComPtr<ID3D11Device> _device, ComPtr<ID3D11DeviceContext> _context) : BasePass(_device, _context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadPixelShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "PS");
	m_shaderManager->LoadVertexShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "VS");

	HRESULT hr = m_device->CreateInputLayout(FSQuadInputLayoutDesc, ARRAYSIZE(FSQuadInputLayoutDesc),
											 m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferPointer(),
											 m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferSize(), &m_inputLayout);
	assert(SUCCEEDED(hr));

	// Create vertex buffer for fullscreen quad
	constexpr float vertices[] = {
		-1.0f, -1.0f, 0.0, 0.0f, 1.0f, // Bottom-left
		1.0f, -1.0f, 0.0, 1.0f, 1.0f,  // Bottom-right
		-1.0f, 1.0f, 0.0, 0.0f, 0.0f,  // Top-left
		1.0f, 1.0f, 0.0, 1.0f, 0.0f	   // Top-right
	};

	m_vertexBuffer = createVertexBuffer(sizeof(vertices), vertices);

	// Create index buffer for fullscreen quad
	const UINT indices[] = {
		0, 1, 2, // First triangle
		1, 3, 2	 // Second triangle
	};
	m_indexBuffer = createIndexBuffer(sizeof(indices), indices);

	m_samplerState = createSamplerState(SamplerPreset::LinearClamp);
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::Disabled);
	createOrResize();
}

void FSQuad::draw(ComPtr<ID3D11ShaderResourceView> srv)
{
	static constexpr UINT stride = 5 * sizeof(float);
	static constexpr UINT offset = 0;

	beginDebugEvent(L"FSQuad Pass");
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_context->VSSetShader(m_shaderManager->getVertexShader("toFSQuad"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("toFSQuad"), nullptr, 0);
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->PSSetShaderResources(0, 1, srv.GetAddressOf());
	m_context->RSSetState(m_rasterizerState.Get());
	m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);


	m_context->DrawIndexed(6, 0, 0);
	if (!m_samplerState)
	{
		std::cerr << "Sampler missing\n";
	}
	unbindRenderTargets(1);
	unbindShaderResources(0, 1);
	unbindComputeUAVs(0, 0);

	endDebugEvent();
}


void FSQuad::createOrResize()
{
	if (m_texture != nullptr)
	{
		m_texture.Reset();
		m_rtv.Reset();
		m_srv.Reset();
	}
	m_texture = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R8G8B8A8_UNORM);
	m_rtv = createRenderTargetView(m_texture.Get(), RTVPreset::Texture2D);
	m_srv = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);

	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);

	D3D11_VIEWPORT viewport;
	viewport.Height = static_cast<float>(AppConfig::getViewportHeight());
	viewport.Width = static_cast<float>(AppConfig::getViewportWidth());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	m_rtvCollector->addRTV("FSQUAD::RTV", m_srv.Get());
	m_context->RSSetViewports(1, &viewport);
}

const ComPtr<ID3D11ShaderResourceView>& FSQuad::getSRV() const
{
	return m_srv;
}
