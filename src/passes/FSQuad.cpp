#include "FSQuad.hpp"
#include <assert.h>
#include <iostream>
#include "debugPassMacros.hpp"
#include "appConfig.hpp"


FSQuad::FSQuad(const ComPtr<ID3D11Device>& _device, const ComPtr<ID3D11DeviceContext>& _context)
	:
	m_device(_device),
	m_context(_context)
{

	m_shaderManager = std::make_unique<ShaderManager>(m_device);
	m_shaderManager->LoadPixelShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "PS");
	m_shaderManager->LoadVertexShader("toFSQuad", L"../../src/shaders/toFSQuad.hlsl", "VS");

	HRESULT hr = m_device->CreateInputLayout(
		FSQuadInputLayoutDesc,
		ARRAYSIZE(FSQuadInputLayoutDesc),
		m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferPointer(),
		m_shaderManager->getVertexShaderBlob("toFSQuad")->GetBufferSize(),
		&m_inputLayout);
	assert(SUCCEEDED(hr));

	// Create vertex buffer for fullscreen quad
	float vertices[] = {
		-1.0f, -1.0f, 0.0, 0.0f, 1.0f,  // Bottom-left
		 1.0f, -1.0f, 0.0, 1.0f, 1.0f,  // Bottom-right
		-1.0f,  1.0f, 0.0, 0.0f, 0.0f,  // Top-left
		 1.0f,  1.0f, 0.0, 1.0f, 0.0f   // Top-right
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
	UINT indices[] = {
		0, 1, 2,  // First triangle
		1, 3, 2   // Second triangle
	};

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(indices);
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	hr = m_device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	assert(SUCCEEDED(hr));

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	{
		HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
		if (FAILED(hr))
			std::cerr << "Failed to create Sampler: HRESULT = " << hr << std::endl;
	}

	createOrResize();

}

void FSQuad::draw(const ComPtr<ID3D11ShaderResourceView>& srv)
{
	static const UINT stride = 5 * sizeof(float);
	static const UINT offset = 0;


	DEBUG_PASS_START(L"FSQuad Draw");

	m_context->VSSetShader(m_shaderManager->getVertexShader("toFSQuad"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("toFSQuad"), nullptr, 0);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->PSSetShaderResources(0, 1, srv.GetAddressOf());
	m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);


	m_context->DrawIndexed(6, 0, 0);
	if (!m_samplerState) {
		std::cerr << "Sampler missing\n";
	}
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_context->PSSetShaderResources(0, 1, &nullSRV);
	m_context->OMSetRenderTargets(1, &nullRTV, nullptr);

	DEBUG_PASS_END();
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
	textureDesc.CPUAccessFlags = 0; // FIX: Remove CPU_ACCESS_READ - not compatible with DEFAULT usage
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // FIX: Use standard 8-bit format for final output
	textureDesc.Height = AppConfig::getViewportHeight(); // FIX: Use window size, not viewport
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

	m_context->RSSetViewports(1, &viewport);
}

const ComPtr<ID3D11ShaderResourceView>& FSQuad::getSRV() const
{
	return m_srv;
}
