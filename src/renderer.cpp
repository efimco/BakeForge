#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "renderer.hpp"
#include "gltfImporter.hpp"
#include "sceneManager.hpp"
#include "appConfig.hpp"




Renderer::Renderer(const HWND& hwnd)
{
	m_device = std::make_unique<DXDevice>(hwnd);
	m_shaderManager = std::make_unique<ShaderManager>(m_device->getDevice());
	m_uiManager = std::make_unique<UIManager>(m_device->getDevice(), m_device->getContext(), hwnd);
	m_objectPicker = std::make_unique<ObjectPicker>(m_device->getDevice(), m_device->getContext());

	glm::vec3 cameraPosition(0.0f, 0.0f, -5.0f);
	m_camera = std::make_unique<Camera>(cameraPosition);
	// Initialize camera matrices

	m_view = m_camera->getViewMatrix();
	float aspectRatio = (float)AppConfig::getViewportWidth() / (float)AppConfig::getViewportHeight();
	m_projection = glm::perspectiveLH(glm::radians(m_camera->zoom), aspectRatio, 0.1f, 100.0f);


	m_prevTime = std::chrono::system_clock::now();
	m_scene = new SceneNode();
	GLTFModel gltfModel(std::string("..\\..\\res\\Knight.glb"), m_device->getDevice(), m_scene);
	std::cout << "Number of primitives loaded: " << SceneManager::getPrimitiveCount() << std::endl;
	m_gBuffer = std::make_unique<GBuffer>(m_device->getDevice(), m_device->getContext());
	m_fsquad = std::make_unique<FSQuad>(m_device->getDevice(), m_device->getContext());
	resize();
}



void Renderer::draw()
{
	if (AppConfig::getNeedsResize())
	{
		resize();
		m_gBuffer->createOrResize();
		m_fsquad->createOrResize();
		AppConfig::setNeedsResize(false);
	}

	// --- CPU Updates ---
	std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
	m_deltaTime = currentTime - m_prevTime;
	m_prevTime = currentTime;

	m_camera->processMovementControls();
	m_view = m_camera->getViewMatrix();

	static int frameCount = 0;
	if (++frameCount % 60 == 0) // Check every 60 frames
	{
		m_shaderManager->checkForChanges();
	}

	// --- GPU Work ---
	m_gBuffer->draw(m_view, m_projection, m_deltaTime.count());
	m_fsquad->draw(m_gBuffer->getAlbedoSRV());
	m_device->getContext()->OMSetRenderTargets(1, m_backBufferRTV.GetAddressOf(), m_depthStencilView.Get());
	m_device->getContext()->ClearRenderTargetView(m_backBufferRTV.Get(), AppConfig::getClearColor());
	m_device->getContext()->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_device->getContext()->RSSetState(m_rasterizerState.Get());
	m_device->getContext()->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_objectPicker->dispatchPick(m_gBuffer->getObjectIDSRV(), m_uiManager->getMousePos());
	m_uiManager->draw(m_fsquad->getSRV(), *m_gBuffer, m_scene);
	m_device->getSwapChain()->Present(1, 0);
}


void Renderer::resize()
{
	m_backBuffer.Reset();
	m_backBufferRTV.Reset();
	m_depthStencilBuffer.Reset();
	m_depthStencilView.Reset();

	float aspectRatio = (float)AppConfig::getViewportWidth() / ((float)AppConfig::getViewportHeight() != 0.0f ? (float)AppConfig::getViewportHeight() : 1.0f);
	m_projection = glm::perspectiveLH(glm::radians(m_camera->zoom), aspectRatio, 0.1f, 100.0f);

	m_device->getSwapChain()->ResizeBuffers(0, AppConfig::getWindowWidth(), AppConfig::getWindowHeight(), DXGI_FORMAT_UNKNOWN, 0);

	{
		HRESULT hr = m_device->getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &m_backBuffer);
		assert(SUCCEEDED(hr));
	}

	{
		HRESULT hr = m_device->getDevice()->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_backBufferRTV);
		assert(SUCCEEDED(hr));
	}

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

		HRESULT hr = m_device->getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &m_depthStencilBuffer);
		assert(SUCCEEDED(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		hr = m_device->getDevice()->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthStencilViewDesc, &m_depthStencilView);
		assert(SUCCEEDED(hr));
	}

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(AppConfig::getWindowWidth());
	viewport.Height = static_cast<float>(AppConfig::getWindowHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	m_device->getContext()->RSSetViewports(1, &viewport);


}