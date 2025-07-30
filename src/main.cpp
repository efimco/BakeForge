#ifndef UNICODE
#define UNICODE
#endif
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <assert.h>
#include <chrono>
#include <ctime>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <vector>
#include <wrl.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include "window.hpp"
#include "camera.hpp"
#include "shaderManager.hpp"
#include "dxDevice.hpp"
#include "gltfImporter.hpp"
#include "sceneManager.hpp"

using namespace Microsoft::WRL;
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	Window window(hInstance);

	DXDevice dxDevice(window.getHandle());

	ShaderManager shaderManager(dxDevice.getDevice());

	ComPtr<ID3D11RasterizerState> rasterizerState;
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_BACK;
		rasterizerDesc.FrontCounterClockwise = false;

		dxDevice.getDevice()->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
	}

	ComPtr<ID3D11DepthStencilState> depthStencilState;
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		dxDevice.getDevice()->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
	}

	ComPtr<ID3D11Texture2D> backBuffer;
	{
		HRESULT hr = dxDevice.getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11RenderTargetView> renderTargetView;
	{
		HRESULT hr = dxDevice.getDevice()->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11Texture2D> depthStencilBuffer;
	ComPtr<ID3D11DepthStencilView> depthStencilView;
	{
		D3D11_TEXTURE2D_DESC depthBufferDesc = {};
		depthBufferDesc.Width = window.width;
		depthBufferDesc.Height = window.height;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		HRESULT hr = dxDevice.getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &depthStencilBuffer);
		assert(SUCCEEDED(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		hr = dxDevice.getDevice()->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, &depthStencilView);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11InputLayout> inputLayout;

	shaderManager.LoadVertexShader("main", L"../../src/shaders/main.hlsl", "VS");
	shaderManager.LoadPixelShader("main", L"../../src/shaders/main.hlsl", "PS");
	{
		HRESULT hr = dxDevice.getDevice()->CreateInputLayout(
			genericInputLayoutDesc,
			ARRAYSIZE(genericInputLayoutDesc),
			shaderManager.getVertexShaderBlob("main")->GetBufferPointer(),
			shaderManager.getVertexShaderBlob("main")->GetBufferSize(),
			&inputLayout
		);
		assert(SUCCEEDED(hr));
	};

	ComPtr<ID3D11Buffer> constantbuffer;

	struct alignas(16) ConstantBufferData
	{
		glm::mat4 modelViewProjection;
		glm::mat4 inverseTransposedModel;
	};

	glm::vec3 cameraPosition(0.0f, 0.0f, -5.0f);
	Camera camera(cameraPosition);
	// Initialize camera matrices

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = glm::perspectiveLH(glm::radians(camera.zoom), (float)window.width / (float)window.height, 0.1f, 100.0f);
	glm::mat4 model = glm::mat4(1.0f);

	glm::mat4 mvp = projection * view * model;

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
		HRESULT hr = dxDevice.getDevice()->CreateBuffer(&constantBufferDesc, &constantBufferResourceData, &constantbuffer);
		assert(SUCCEEDED(hr));
	}

	std::string texname = std::string("..\\..\\res\\pattern.jpg");
	if (!std::filesystem::exists(texname))
	{
		std::cerr << "Texture file not found: " << texname << std::endl;
	}
	int twidth, theight;
	unsigned char* data = stbi_load(texname.c_str(), &twidth, &theight, nullptr, 4);
	if (!data)
	{
		std::cerr << "Failed to load texture: " << texname << std::endl;
	}


	D3D11_TEXTURE2D_DESC t2ddesc;
	t2ddesc.Width = twidth;
	t2ddesc.Height = theight;
	t2ddesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	t2ddesc.MipLevels = 1;
	t2ddesc.ArraySize = 1;
	t2ddesc.Usage = D3D11_USAGE_DEFAULT;
	t2ddesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	t2ddesc.CPUAccessFlags = 0;
	t2ddesc.MiscFlags = 0;
	t2ddesc.SampleDesc.Count = 1;
	t2ddesc.SampleDesc.Quality = 0;


	D3D11_SUBRESOURCE_DATA mipData;
	mipData.pSysMem = static_cast<void*>(data);
	mipData.SysMemPitch = twidth * 4;
	mipData.SysMemSlicePitch = 0;

	ComPtr<ID3D11Texture2D> tex;

	{
		HRESULT hr = dxDevice.getDevice()->CreateTexture2D(&t2ddesc, &mipData, &tex);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create texture: HRESULT = " << hr << std::endl;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = t2ddesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = t2ddesc.MipLevels;

	ComPtr<ID3D11ShaderResourceView> srv4t2d;
	{
		HRESULT hr = dxDevice.getDevice()->CreateShaderResourceView(tex.Get(), &srvDesc, &srv4t2d);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create SRV: HRESULT = " << hr << std::endl;
		}
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

	ComPtr<ID3D11SamplerState> sampler;
	{
		HRESULT hr = dxDevice.getDevice()->CreateSamplerState(&sdesc, &sampler);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create sampler: HRESULT = " << hr << std::endl;
		}
	}

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(window.width);
	viewport.Height = static_cast<float>(window.height);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	dxDevice.getContext()->RSSetViewports(1, &viewport);

	std::chrono::system_clock::time_point prevTime = std::chrono::system_clock::now();
	std::chrono::duration<double> deltaTime;

	GLTFModel gltfModel(std::string("..\\..\\res\\Knight.glb"), dxDevice.getDevice());

	std::cout << "Number of primitives loaded: " << SceneManager::getPrimitiveCount() << std::endl;

	MSG message = {};
	while (message.message != WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_SIZE)
			{
				// Handle window resize
				if (dxDevice.getSwapChain())
				{
					renderTargetView.Reset();
					backBuffer.Reset();
					depthStencilView.Reset();
					depthStencilBuffer.Reset();

					HRESULT hr = dxDevice.getSwapChain()->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
					assert(SUCCEEDED(hr));

					hr = dxDevice.getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
					assert(SUCCEEDED(hr));

					hr = dxDevice.getDevice()->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
					assert(SUCCEEDED(hr));

					D3D11_TEXTURE2D_DESC depthBufferDesc = {};
					depthBufferDesc.Width = window.width;
					depthBufferDesc.Height = window.height;
					depthBufferDesc.MipLevels = 1;
					depthBufferDesc.ArraySize = 1;
					depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					depthBufferDesc.SampleDesc.Count = 1;
					depthBufferDesc.SampleDesc.Quality = 0;
					depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
					depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

					hr = dxDevice.getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &depthStencilBuffer);
					assert(SUCCEEDED(hr));

					D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
					depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					depthStencilViewDesc.Texture2D.MipSlice = 0;

					hr = dxDevice.getDevice()->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, &depthStencilView);
					assert(SUCCEEDED(hr));

					viewport.Width = static_cast<float>(window.width);
					viewport.Height = static_cast<float>(window.height);
					dxDevice.getContext()->RSSetViewports(1, &viewport);

					projection = glm::perspective(glm::radians(camera.zoom), (float)window.width / (float)window.height, 0.1f, 100.0f);

					glm::mat4 mvp = projection * view * model;

					D3D11_MAPPED_SUBRESOURCE mappedResource;
					hr = dxDevice.getContext()->Map(constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
					if (SUCCEEDED(hr))
					{
						ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
						cbData->modelViewProjection = glm::transpose(mvp);
						dxDevice.getContext()->Unmap(constantbuffer.Get(), 0);
					}
				}
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		static int frameCount = 0;
		if (++frameCount % 60 == 0) // Check every 60 frames
		{
			shaderManager.checkForChanges();
		}

		std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
		deltaTime = currentTime - prevTime;
		// std::cout << deltaTime.count() << std::endl;
		prevTime = currentTime;

		static float rotationY = 0.0f;
		rotationY += static_cast<float>(deltaTime.count());
		model = glm::rotate(glm::mat4(1.0f), rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 mvp = projection * view * model;
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = dxDevice.getContext()->Map(constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr))
		{
			ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
			cbData->modelViewProjection = glm::transpose(mvp);
			cbData->inverseTransposedModel = glm::transpose(glm::inverse(model));
			dxDevice.getContext()->Unmap(constantbuffer.Get(), 0);
		}

		dxDevice.getContext()->RSSetState(rasterizerState.Get());
		dxDevice.getContext()->OMSetDepthStencilState(depthStencilState.Get(), 0);
		dxDevice.getContext()->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
		float clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f }; // blue
		dxDevice.getContext()->ClearRenderTargetView(renderTargetView.Get(), clearColor);
		dxDevice.getContext()->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		dxDevice.getContext()->VSSetShader(shaderManager.getVertexShader("main"), nullptr, 0);
		dxDevice.getContext()->PSSetShader(shaderManager.getPixelShader("main"), nullptr, 0);

		dxDevice.getContext()->VSSetConstantBuffers(0, 1, constantbuffer.GetAddressOf());

		dxDevice.getContext()->PSSetSamplers(0, 1, sampler.GetAddressOf());


		dxDevice.getContext()->IASetInputLayout(inputLayout.Get());
		dxDevice.getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		UINT stride = sizeof(InterleavedData);
		UINT offset = 0;
		for (auto& prim : SceneManager::getPrimitives())
		{
			dxDevice.getContext()->IASetVertexBuffers(0, 1, prim.getVertexBuffer().GetAddressOf(), &stride, &offset);
			dxDevice.getContext()->IASetIndexBuffer(prim.getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
			auto& material = prim.getMaterial();
			dxDevice.getContext()->PSSetShaderResources(0, 1, material->albedo->srv.GetAddressOf());
			dxDevice.getContext()->DrawIndexed(static_cast<UINT>(prim.getIndexData().size()), 0, 0);
		}

		dxDevice.getSwapChain()->Present(1, 0);
	}
	return 0;
}
