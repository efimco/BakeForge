#include "FSQuad.hpp"

#include <cassert>
#include <iostream>

#include "appConfig.hpp"
#include "rtvCollector.hpp"
#include "shaderManager.hpp"

FSQuad::FSQuad(ComPtr<ID3D11Device> _device, ComPtr<ID3D11DeviceContext> _context) : BasePass(_device, _context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadPixelShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "PS");
	m_shaderManager->LoadVertexShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "VS");

	HRESULT hr =
		m_device->CreateInputLayout(FSQuadInputLayoutDesc, ARRAYSIZE(FSQuadInputLayoutDesc),
									m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferPointer(),
									m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferSize(), &m_inputLayout);
	assert(SUCCEEDED(hr));

	// Create vertex buffer for fullscreen quad
	constexpr float vertices[] = {
		-1.0f, -1.0f, 0.0, 0.0f, 1.0f, // Bottom-left
		1.0f,  -1.0f, 0.0, 1.0f, 1.0f, // Bottom-right
		-1.0f, 1.0f,  0.0, 0.0f, 0.0f, // Top-left
		1.0f,  1.0f,  0.0, 1.0f, 0.0f  // Top-right
	};

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(vertices);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices;

	hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	assert(SUCCEEDED(hr));

	// Create index buffer for fullscreen quad
	const UINT indices[] = {
		0, 1, 2, // First triangle
		1, 3, 2	 // Second triangle
	};

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(indices);
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	hr = m_device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	assert(SUCCEEDED(hr));

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
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11RenderTargetView* nullRTV = nullptr;
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
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Height = AppConfig::getViewportHeight();
	textureDesc.Width = AppConfig::getViewportWidth();
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_texture);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo SRV: " << hr << std::endl;
	}

	D3D11_VIEWPORT viewport;
	viewport.Height = static_cast<float>(AppConfig::getViewportHeight());
	viewport.Width = static_cast<float>(AppConfig::getViewportWidth());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.FrontCounterClockwise = false;

	HRESULT hr = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
	if (FAILED(hr))
		std::cerr << "Failed to create Rasterizer State: HRESULT = " << hr << std::endl;

	m_rtvCollector->addRTV("FSQUAD::RTV", m_srv.Get());
	m_context->RSSetViewports(1, &viewport);
}

const ComPtr<ID3D11ShaderResourceView>& FSQuad::getSRV() const
{
	return m_srv;
}
