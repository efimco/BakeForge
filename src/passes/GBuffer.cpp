#include "GBuffer.hpp"
#include "appConfig.hpp"
#include <iostream>
#include "sceneManager.hpp"s

GBuffer::GBuffer(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : m_device(device), m_context(context)
{
	D3D11_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;

	{
		HRESULT hr = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
		if (FAILED(hr))
			std::cerr << "Error Creating Rasterizer State: " << hr << std::endl;
	}

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	{
		HRESULT hr = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
		if (FAILED(hr))
			std::cerr << "Error Creating Depths Stencil State: " << hr << std::endl;
	}

	createOrResize();

	m_rtvs[0] = rtv_albedo.Get();
	m_rtvs[1] = rtv_metallicRoughness.Get();
	m_rtvs[2] = rtv_normal.Get();
	m_rtvs[3] = rtv_position.Get();

	m_shaderManager = new ShaderManager(device);

	m_shaderManager->LoadPixelShader("gBuffer", L"../../src/shaders/GBuffer.hlsl", "PS");
	m_shaderManager->LoadVertexShader("gBuffer", L"../../src/shaders/GBuffer.hlsl", "VS");

	{
		HRESULT hr = m_device->CreateInputLayout(
			genericInputLayoutDesc,
			ARRAYSIZE(genericInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};


	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 0;

	{
		HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
		if (FAILED(hr))
			std::cerr << "Failed to create Sampler: HRESULT = " << hr << std::endl;
	}
}


void GBuffer::draw(glm::mat4 view, glm::mat4 projection)
{

	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(4, m_rtvs, dsv.Get());

	for (auto rtv : m_rtvs)
	{
		m_context->ClearRenderTargetView(rtv, AppConfig::getClearColor());
	}

	m_context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("main"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());

	m_context->PSSetShader(m_shaderManager->getPixelShader("main"), nullptr, 0);
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	UINT stride = sizeof(InterleavedData);
	UINT offset = 0;
	for (auto& prim : SceneManager::getPrimitives())
	{
		m_context->IASetVertexBuffers(0, 1, prim.getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim.getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		auto& material = prim.getMaterial();
		m_context->PSSetShaderResources(0, 1, material->albedo->srv.GetAddressOf());
		m_context->DrawIndexed(static_cast<UINT>(prim.getIndexData().size()), 0, 0);
	}

}

void GBuffer::createOrResize()
{

	if (t_albedo != nullptr)
	{
		t_albedo.Reset();
		t_metallicRoughness.Reset();
		t_normal.Reset();
		t_position.Reset();
		t_depth.Reset();

		rtv_albedo.Reset();
		rtv_metallicRoughness.Reset();
		dsv.Reset();
		rtv_normal.Reset();
		rtv_position.Reset();

		srv_albedo.Reset();
		srv_metallicRoughness.Reset();
		srv_depth.Reset();
		srv_normal.Reset();
		srv_position.Reset();
	}

	//albedo
	D3D11_TEXTURE2D_DESC albedoDesc;
	albedoDesc.ArraySize = 1;
	albedoDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	albedoDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	albedoDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	albedoDesc.Height = AppConfig::getWindowHeight();
	albedoDesc.Width = AppConfig::getWindowWidth();
	albedoDesc.MipLevels = 1;
	albedoDesc.MiscFlags = 0;
	albedoDesc.SampleDesc.Count = 1;
	albedoDesc.SampleDesc.Quality = 0;
	albedoDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC albedoSRVDesc = {};
	albedoSRVDesc.Format = albedoDesc.Format;
	albedoSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	albedoSRVDesc.Texture2D.MostDetailedMip = 0;
	albedoSRVDesc.Texture2D.MipLevels = albedoDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&albedoDesc, nullptr, &t_albedo);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(t_albedo.Get(), nullptr, &rtv_albedo);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_albedo.Get(), &albedoSRVDesc, &srv_albedo);
		if (FAILED(hr))
			std::cerr << "Error Creating Albedo SRV: " << hr << std::endl;
	}

	//metallicRoughness
	D3D11_TEXTURE2D_DESC metallicRoughnessDesc;
	metallicRoughnessDesc.ArraySize = 1;
	metallicRoughnessDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	metallicRoughnessDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	metallicRoughnessDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	metallicRoughnessDesc.Height = AppConfig::getWindowHeight();
	metallicRoughnessDesc.Width = AppConfig::getWindowWidth();
	metallicRoughnessDesc.MipLevels = 1;
	metallicRoughnessDesc.MiscFlags = 0;
	metallicRoughnessDesc.SampleDesc.Count = 1;
	metallicRoughnessDesc.SampleDesc.Quality = 0;
	metallicRoughnessDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC metallicRoughnessSRVDesc = {};
	metallicRoughnessSRVDesc.Format = albedoDesc.Format;
	metallicRoughnessSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	metallicRoughnessSRVDesc.Texture2D.MostDetailedMip = 0;
	metallicRoughnessSRVDesc.Texture2D.MipLevels = albedoDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&metallicRoughnessDesc, nullptr, &t_metallicRoughness);
		if (FAILED(hr))
			std::cerr << "Error Creating MetallicRoughness Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(t_metallicRoughness.Get(), nullptr, &rtv_metallicRoughness);
		if (FAILED(hr))
			std::cerr << "Error Creating MetallicRoughness RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_metallicRoughness.Get(), &metallicRoughnessSRVDesc, &srv_metallicRoughness);
		if (FAILED(hr))
			std::cerr << "Error Creating MetallicRoughness SRV: " << hr << std::endl;
	}


	//normal
	D3D11_TEXTURE2D_DESC normalDesc;
	normalDesc.ArraySize = 1;
	normalDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	normalDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	normalDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	normalDesc.Height = AppConfig::getWindowHeight();
	normalDesc.Width = AppConfig::getWindowWidth();
	normalDesc.MipLevels = 1;
	normalDesc.MiscFlags = 0;
	normalDesc.SampleDesc.Count = 1;
	normalDesc.SampleDesc.Quality = 0;
	normalDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC normalSRVDesc = {};
	normalSRVDesc.Format = normalDesc.Format;
	normalSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	normalSRVDesc.Texture2D.MostDetailedMip = 0;
	normalSRVDesc.Texture2D.MipLevels = normalDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&normalDesc, nullptr, &t_normal);
		if (FAILED(hr))
			std::cerr << "Error Creating Normal Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(t_normal.Get(), nullptr, &rtv_normal);
		if (FAILED(hr))
			std::cerr << "Error Creating Normal RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_normal.Get(), &normalSRVDesc, &srv_normal);
		if (FAILED(hr))
			std::cerr << "Error Creating Normal SRV: " << hr << std::endl;
	}

	//position
	D3D11_TEXTURE2D_DESC positionDesc;
	positionDesc.ArraySize = 1;
	positionDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	positionDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	positionDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	positionDesc.Height = AppConfig::getWindowHeight();
	positionDesc.Width = AppConfig::getWindowWidth();
	positionDesc.MipLevels = 1;
	positionDesc.MiscFlags = 0;
	positionDesc.SampleDesc.Count = 1;
	positionDesc.SampleDesc.Quality = 0;
	positionDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC positionSRVDesc = {};
	positionSRVDesc.Format = normalDesc.Format;
	positionSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	positionSRVDesc.Texture2D.MostDetailedMip = 0;
	positionSRVDesc.Texture2D.MipLevels = normalDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&positionDesc, nullptr, &t_position);
		if (FAILED(hr))
			std::cerr << "Error Creating Position Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(t_position.Get(), nullptr, &rtv_position);
		if (FAILED(hr))
			std::cerr << "Error Creating Position RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_position.Get(), &positionSRVDesc, &srv_normal);
		if (FAILED(hr))
			std::cerr << "Error Creating Position SRV: " << hr << std::endl;
	}

	//depth
	D3D11_TEXTURE2D_DESC depthDesc;
	depthDesc.ArraySize = 1;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	depthDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	depthDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	depthDesc.Height = AppConfig::getWindowHeight();
	depthDesc.Width = AppConfig::getWindowWidth();
	depthDesc.MipLevels = 1;
	depthDesc.MiscFlags = 0;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	{
		HRESULT hr = m_device->CreateTexture2D(&depthDesc, nullptr, &t_depth);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthSrencil Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateDepthStencilView(t_depth.Get(), &dsvDesc, &dsv);
		if (FAILED(hr))
			std::cerr << "Error Creating DepthSrencil DSV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_depth.Get(), &positionSRVDesc, &srv_normal);
		if (FAILED(hr))
			std::cerr << "Error Creating PosDepthSrencilition SRV: " << hr << std::endl;
	}

}