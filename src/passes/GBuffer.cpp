#include "GBuffer.hpp"
#include "appConfig.hpp"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include "debugPassMacros.hpp"
#include "material.hpp"
#include "texture.hpp"
#include "scene.hpp"
#include "GBufferTextures.hpp"
#include "primitive.hpp"
#include "shaderManager.hpp"

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
	glm::mat4 inverseTransposedModel;
	glm::mat4 model;
	int objectID;
	float padding[3]; // Align to 16 bytes
	glm::vec3 cameraPosition;
	float padding2; // Align to 16 bytes
};

GBuffer::GBuffer(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : m_device(device), m_context(context)
{
	D3D11_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.DepthClipEnable = false;
	rasterizerDesc.ScissorEnable = false;

	{
		HRESULT hr = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
		if (FAILED(hr))
			std::cerr << "Error Creating Rasterizer State: " << hr << std::endl;
	}

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;

	{
		HRESULT hr = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
		if (FAILED(hr))
			std::cerr << "Error Creating Depths Stencil State: " << hr << std::endl;
	}

	createOrResize();

	m_shaderManager = std::make_unique<ShaderManager>(device);

	m_shaderManager->LoadPixelShader("gBuffer", L"../../src/shaders/gBuffer.hlsl", "PS");
	m_shaderManager->LoadVertexShader("gBuffer", L"../../src/shaders/gBuffer.hlsl", "VS");

	{
		HRESULT hr = m_device->CreateInputLayout(
			genericInputLayoutDesc,
			ARRAYSIZE(genericInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};


	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 16;

	{
		HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
		if (FAILED(hr))
			std::cerr << "Failed to create Sampler: HRESULT = " << hr << std::endl;
	}


	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.StructureByteStride = 0;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	{
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantbuffer);
		assert(SUCCEEDED(hr));
	}
}

void GBuffer::draw(const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	Scene* scene,
	ComPtr<ID3D11DepthStencilView>& dsv)
{

	DEBUG_PASS_START(L"GBuffer Draw");
	if (AppConfig::getNeedsResize())
	{
		createOrResize();
	}
	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(5, m_rtvs, dsv.Get());

	for (auto rtv : m_rtvs)
	{
		m_context->ClearRenderTargetView(rtv, AppConfig::getClearColor());
	}

	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("gBuffer"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());

	m_context->PSSetShader(m_shaderManager->getPixelShader("gBuffer"), nullptr, 0);
	m_context->PSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	static const UINT stride = sizeof(InterleavedData);
	static const UINT offset = 0;
	for (int i = 0; i < scene->getPrimitiveCount(); i++)
	{
		auto objectID = i;
		Primitive* prim = scene->getPrimitives()[i];
		if (!prim->material)
		{
			std::cerr << "Primitive " << i << " has no material!" << std::endl;
			continue;
		}
		update(view, projection, cameraPosition, scene, objectID, prim);
		m_context->IASetVertexBuffers(0, 1, prim->getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim->getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		ID3D11ShaderResourceView* const* SRVs = prim->material->getSRVs();
		m_context->PSSetShaderResources(0, 3, SRVs);
		m_context->DrawIndexed(static_cast<UINT>(prim->getIndexData().size()), 0, 0);
	}
	ID3D11RenderTargetView* nullRTVs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	m_context->OMSetRenderTargets(5, nullRTVs, nullptr);
	DEBUG_PASS_END();

}

void GBuffer::update(const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	Scene* scene,
	int objectID,
	Primitive* prim)
{
	glm::mat4 model = prim->getWorldMatrix();
	glm::mat4 mvp = projection * view * model;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		// Use inverse-transpose of the model matrix (upper-left 3x3) for correct normal transformation
		cbData->inverseTransposedModel = glm::transpose(glm::inverse(model));
		cbData->model = glm::transpose(model);
		cbData->objectID = objectID + 1;
		cbData->cameraPosition = cameraPosition;
		m_context->Unmap(m_constantbuffer.Get(), 0);
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

		rtv_albedo.Reset();
		rtv_metallicRoughness.Reset();
		rtv_normal.Reset();
		rtv_position.Reset();

		srv_albedo.Reset();
		srv_metallicRoughness.Reset();
		srv_normal.Reset();
		srv_position.Reset();
	}

	//albedo
	D3D11_TEXTURE2D_DESC albedoDesc = {};
	albedoDesc.ArraySize = 1;
	albedoDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	albedoDesc.CPUAccessFlags = 0;
	albedoDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	albedoDesc.Height = AppConfig::getViewportHeight();
	albedoDesc.Width = AppConfig::getViewportWidth();
	albedoDesc.MipLevels = 1;
	albedoDesc.MiscFlags = 0;
	albedoDesc.SampleDesc.Count = 1;
	albedoDesc.SampleDesc.Quality = 0;
	albedoDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC albedoSRVDesc;
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
	metallicRoughnessDesc.CPUAccessFlags = 0;
	metallicRoughnessDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	metallicRoughnessDesc.Height = AppConfig::getViewportHeight();
	metallicRoughnessDesc.Width = AppConfig::getViewportWidth();
	metallicRoughnessDesc.MipLevels = 1;
	metallicRoughnessDesc.MiscFlags = 0;
	metallicRoughnessDesc.SampleDesc.Count = 1;
	metallicRoughnessDesc.SampleDesc.Quality = 0;
	metallicRoughnessDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC metallicRoughnessSRVDesc = {};
	metallicRoughnessSRVDesc.Format = metallicRoughnessDesc.Format;
	metallicRoughnessSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	metallicRoughnessSRVDesc.Texture2D.MostDetailedMip = 0;
	metallicRoughnessSRVDesc.Texture2D.MipLevels = metallicRoughnessDesc.MipLevels;

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
	normalDesc.CPUAccessFlags = 0;
	normalDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	normalDesc.Height = AppConfig::getViewportHeight();
	normalDesc.Width = AppConfig::getViewportWidth();
	normalDesc.MipLevels = 1;
	normalDesc.MiscFlags = 0;
	normalDesc.SampleDesc.Count = 1;
	normalDesc.SampleDesc.Quality = 0;
	normalDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC normalSRVDesc;
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
	positionDesc.CPUAccessFlags = 0;
	positionDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	positionDesc.Height = AppConfig::getViewportHeight();
	positionDesc.Width = AppConfig::getViewportWidth();
	positionDesc.MipLevels = 1;
	positionDesc.MiscFlags = 0;
	positionDesc.SampleDesc.Count = 1;
	positionDesc.SampleDesc.Quality = 0;
	positionDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC positionSRVDesc;
	positionSRVDesc.Format = positionDesc.Format;
	positionSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	positionSRVDesc.Texture2D.MostDetailedMip = 0;
	positionSRVDesc.Texture2D.MipLevels = positionDesc.MipLevels;

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
		HRESULT hr = m_device->CreateShaderResourceView(t_position.Get(), &positionSRVDesc, &srv_position);
		if (FAILED(hr))
			std::cerr << "Error Creating Position SRV: " << hr << std::endl;
	}

	//objectID
	D3D11_TEXTURE2D_DESC objectIDDesc;
	objectIDDesc.ArraySize = 1;
	objectIDDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	objectIDDesc.CPUAccessFlags = 0;
	objectIDDesc.Format = DXGI_FORMAT_R32_UINT;
	objectIDDesc.Height = AppConfig::getViewportHeight();
	objectIDDesc.Width = AppConfig::getViewportWidth();
	objectIDDesc.MipLevels = 1;
	objectIDDesc.MiscFlags = 0;
	objectIDDesc.SampleDesc.Count = 1;
	objectIDDesc.SampleDesc.Quality = 0;
	objectIDDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC objectIDSRVDesc;
	objectIDSRVDesc.Format = objectIDDesc.Format;
	objectIDSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	objectIDSRVDesc.Texture2D.MostDetailedMip = 0;
	objectIDSRVDesc.Texture2D.MipLevels = objectIDDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&objectIDDesc, nullptr, &t_objectID);
		assert(SUCCEEDED(hr));
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(t_objectID.Get(), nullptr, &rtv_objectID);
		assert(SUCCEEDED(hr));
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(t_objectID.Get(), &objectIDSRVDesc, &srv_objectID);
		assert(SUCCEEDED(hr));
	}

	// rtv assignment
	m_rtvs[0] = rtv_albedo.Get();
	m_rtvs[1] = rtv_metallicRoughness.Get();
	m_rtvs[2] = rtv_normal.Get();
	m_rtvs[3] = rtv_position.Get();
	m_rtvs[4] = rtv_objectID.Get();

}

GBufferTextures GBuffer::getGBufferTextures() const
{
	GBufferTextures gbufferTextures;
	gbufferTextures.albedoSRV = srv_albedo;
	gbufferTextures.metallicRoughnessSRV = srv_metallicRoughness;
	gbufferTextures.normalSRV = srv_normal;
	gbufferTextures.positionSRV = srv_position;
	gbufferTextures.objectIDSRV = srv_objectID;
	return gbufferTextures;
}

const ComPtr<ID3D11ShaderResourceView>& GBuffer::getAlbedoSRV() const
{
	return srv_albedo;
}

const ComPtr<ID3D11ShaderResourceView>& GBuffer::getMetallicRoughnessSRV() const
{
	return srv_metallicRoughness;
}

const ComPtr<ID3D11ShaderResourceView>& GBuffer::getNormalSRV() const
{
	return srv_normal;
}

const ComPtr<ID3D11ShaderResourceView>& GBuffer::getPositionSRV() const
{
	return srv_position;
}

const ComPtr<ID3D11ShaderResourceView>& GBuffer::getObjectIDSRV() const
{
	return srv_objectID;
}
