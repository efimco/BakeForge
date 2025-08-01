#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "renderer.hpp"
#include "camera.hpp"
#include "gltfImporter.hpp"
#include "sceneManager.hpp"
#include "appConfig.hpp"

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
	glm::mat4 inverseTransposedModel;
};

Renderer::Renderer(const HWND &hwnd)
{
	m_device = new DXDevice(hwnd);
	m_shaderManager = new ShaderManager(m_device->getDevice());
	m_uiManager = new UIManager(m_device->getDevice(), m_device->getContext(), hwnd);

	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_BACK;
		rasterizerDesc.FrontCounterClockwise = false;

		m_device->getDevice()->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		m_device->getDevice()->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
	}

	ComPtr<ID3D11Texture2D> backBuffer;
	{
		HRESULT hr = m_device->getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
		assert(SUCCEEDED(hr));
	}

	{
		HRESULT hr = m_device->getDevice()->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11Texture2D> depthStencilBuffer;
	{
		D3D11_TEXTURE2D_DESC depthBufferDesc = {};
		depthBufferDesc.Width = AppConfig::getWindowWidth();
		depthBufferDesc.Height = AppConfig::getWindowHeight();
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		HRESULT hr = m_device->getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &depthStencilBuffer);
		assert(SUCCEEDED(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		hr = m_device->getDevice()->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, &m_depthStencilView);
		assert(SUCCEEDED(hr));
	}

	m_shaderManager->LoadPixelShader("main", L"../../src/shaders/main.hlsl", "PS");
	m_shaderManager->LoadVertexShader("main", L"../../src/shaders/main.hlsl", "VS");
	{
		HRESULT hr = m_device->getDevice()->CreateInputLayout(
			genericInputLayoutDesc,
			ARRAYSIZE(genericInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("main")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("main")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};

	glm::vec3 cameraPosition(0.0f, 0.0f, -5.0f);
	Camera camera(cameraPosition);
	// Initialize camera matrices

	m_view = camera.getViewMatrix();
	m_projection = glm::perspectiveLH(glm::radians(camera.zoom),
									  (float)AppConfig::getWindowWidth() / (float)AppConfig::getWindowHeight(),
									  0.1f, 100.0f);
	glm::mat4 model = glm::mat4(1.0f);

	glm::mat4 mvp = m_projection * m_view * model;

	ConstantBufferData cbdata;
	cbdata.modelViewProjection = glm::transpose(mvp);
	cbdata.inverseTransposedModel = glm::transpose(glm::inverse(model));
	{
		D3D11_BUFFER_DESC constantBufferDesc;
		constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.StructureByteStride = 0;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA constantBufferResourceData;
		constantBufferResourceData.pSysMem = &cbdata;
		constantBufferResourceData.SysMemSlicePitch = 0;
		constantBufferResourceData.SysMemPitch = 0;
		HRESULT hr = m_device->getDevice()->CreateBuffer(&constantBufferDesc, &constantBufferResourceData, &m_constantbuffer);
		assert(SUCCEEDED(hr));
	}

	D3D11_SAMPLER_DESC sdesc;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.MinLOD = -FLT_MAX;
	sdesc.MaxLOD = FLT_MAX;
	sdesc.MipLODBias = 0.0f;
	sdesc.MaxAnisotropy = 0;

	{
		HRESULT hr = m_device->getDevice()->CreateSamplerState(&sdesc, &m_samplerState);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create sampler: HRESULT = " << hr << std::endl;
		}
	}

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(AppConfig::getWindowWidth());
	viewport.Height = static_cast<float>(AppConfig::getWindowHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	m_device->getContext()->RSSetViewports(1, &viewport);

	m_prevTime = std::chrono::system_clock::now();

	GLTFModel gltfModel(std::string("..\\..\\res\\Knight.glb"), m_device->getDevice());

	std::cout << "Number of primitives loaded: " << SceneManager::getPrimitiveCount() << std::endl;
}

Renderer::~Renderer()
{
	m_device->~DXDevice();
}

void Renderer::draw()
{
	if (AppConfig::getNeedsResize())
	{
		resize();
		AppConfig::setNeedsResize(false);
	}

	m_uiManager->beginDraw();
	static int frameCount = 0;
	if (++frameCount % 60 == 0) // Check every 60 frames
	{
		m_shaderManager->checkForChanges();
	}

	std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
	m_deltaTime = currentTime - m_prevTime;
	// std::cout << deltaTime.count() << std::endl;
	m_prevTime = currentTime;

	static float rotationY = 0.0f;
	rotationY += static_cast<float>(m_deltaTime.count());

	glm::mat4 model = glm::rotate(glm::mat4(1.0f), rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 mvp = m_projection * m_view * model;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_device->getContext()->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		ConstantBufferData *cbData = static_cast<ConstantBufferData *>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		cbData->inverseTransposedModel = glm::transpose(glm::inverse(model));
		m_device->getContext()->Unmap(m_constantbuffer.Get(), 0);
	}

	m_device->getContext()->RSSetState(m_rasterizerState.Get());
	m_device->getContext()->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_device->getContext()->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	float clearColor[4] = {0.4f, 0.6f, 0.9f, 1.0f}; // blue
	m_device->getContext()->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_device->getContext()->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	m_device->getContext()->VSSetShader(m_shaderManager->getVertexShader("main"), nullptr, 0);
	m_device->getContext()->PSSetShader(m_shaderManager->getPixelShader("main"), nullptr, 0);
	m_device->getContext()->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	m_device->getContext()->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());

	m_device->getContext()->IASetInputLayout(m_inputLayout.Get());
	m_device->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(InterleavedData);
	UINT offset = 0;
	for (auto &prim : SceneManager::getPrimitives())
	{
		m_device->getContext()->IASetVertexBuffers(0, 1, prim.getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_device->getContext()->IASetIndexBuffer(prim.getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		auto &material = prim.getMaterial();
		m_device->getContext()->PSSetShaderResources(0, 1, material->albedo->srv.GetAddressOf());
		m_device->getContext()->DrawIndexed(static_cast<UINT>(prim.getIndexData().size()), 0, 0);
	}
	m_uiManager->endDraw();
	m_device->getSwapChain()->Present(1, 0);
}

void Renderer::resize()
{
	m_renderTargetView.Reset();
	m_depthStencilView.Reset();

	m_device->getSwapChain()->ResizeBuffers(0, AppConfig::getWindowWidth(), AppConfig::getWindowHeight(), DXGI_FORMAT_UNKNOWN, 0);

	ComPtr<ID3D11Texture2D> backBuffer;
	{
		HRESULT hr = m_device->getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
		assert(SUCCEEDED(hr));
	}

	{
		HRESULT hr = m_device->getDevice()->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11Texture2D> depthStencilBuffer;
	{
		D3D11_TEXTURE2D_DESC depthBufferDesc = {};
		depthBufferDesc.Width = AppConfig::getWindowWidth();
		depthBufferDesc.Height = AppConfig::getWindowHeight();
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		HRESULT hr = m_device->getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &depthStencilBuffer);
		assert(SUCCEEDED(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		hr = m_device->getDevice()->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, &m_depthStencilView);
		assert(SUCCEEDED(hr));
	}
}
