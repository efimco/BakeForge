#include "shaderManager.hpp"
#include <iostream>
#include "appConfig.hpp"
#include "debugPassMacros.hpp"
#include "light.hpp"
#include "scene.hpp"
#include "texture.hpp"
#include "wordSpaceUIPass.hpp"

#define MAX_LIGHTS 100

struct QuadVertex
{
	glm::vec2 cornerPos;
	glm::vec2 uv;
};

struct alignas(16) ConstantBufferData
{
	glm::mat4 viewProjection;
	glm::mat4 view;
	glm::vec3 cameraPosition;
	float sizeInPixels;
	glm::vec2 screenSize;
	uint32_t primitiveCount;
	float padding; // align to 16 bytes
};


static const D3D11_INPUT_ELEMENT_DESC s_WSUIInputLayoutDesc[] =
{
	{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


WorldSpaceUIPass::WorldSpaceUIPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	m_device = device;
	m_context = context;
	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_lightIconTexture = std::make_shared<Texture>("../../res/icons/PointLight.png", m_device);
	m_shaderManager->LoadPixelShader("wordSpaceUIPS", L"../../src/shaders/wordSpaceUI.hlsl", "PS");
	m_shaderManager->LoadVertexShader("wordSpaceUIVS", L"../../src/shaders/wordSpaceUI.hlsl", "VS");
	createOrResize();
	createQuad();
	createLightsBuffer();
	createInputLayout();
	createRasterizerState();
	createDepthStencilState();
	createSamplerState();
	createConstantBuffer();
}

void WorldSpaceUIPass::createOrResize()
{

	if (m_texture != nullptr)
	{
		m_texture.Reset();
		m_rtv.Reset();
		m_srv.Reset();
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Height = AppConfig::getViewportHeight();
	texDesc.Width = AppConfig::getViewportWidth();
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

	{
		HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_texture);
		if (FAILED(hr))
			std::cerr << "Error Creating WSUI Texture: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
		if (FAILED(hr))
			std::cerr << "Error Creating WSUI RTV: " << hr << std::endl;
	}
	{
		HRESULT hr = m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
		if (FAILED(hr))
			std::cerr << "Error Creating WSUI SRV: " << hr << std::endl;
	}
}

void WorldSpaceUIPass::createInputLayout()
{
	HRESULT hr = m_device->CreateInputLayout(
		s_WSUIInputLayoutDesc,
		ARRAYSIZE(s_WSUIInputLayoutDesc),
		m_shaderManager->getVertexShaderBlob("wordSpaceUIVS")->GetBufferPointer(),
		m_shaderManager->getVertexShaderBlob("wordSpaceUIVS")->GetBufferSize(),
		&m_inputLayout);
	assert(SUCCEEDED(hr));

}

void WorldSpaceUIPass::createSamplerState()
{
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
}

void WorldSpaceUIPass::createConstantBuffer()
{
	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.StructureByteStride = 0;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	{
		HRESULT hr = m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
		assert(SUCCEEDED(hr));
	}
}

void WorldSpaceUIPass::updateConstantBuffer(const glm::mat4& view, const glm::mat4& projection, Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(SUCCEEDED(hr));

	ConstantBufferData* dataPtr = (ConstantBufferData*)mappedResource.pData;
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 vp = projection * view;
	dataPtr->viewProjection = glm::transpose(vp);
	dataPtr->view = glm::transpose(view);
	dataPtr->cameraPosition = glm::vec3(scene->transform.position);
	dataPtr->sizeInPixels = 32.0f; // Example value, adjust as needed
	glm::ivec2 screenSize = glm::ivec2(AppConfig::getViewportWidth(), AppConfig::getViewportHeight());
	dataPtr->screenSize = screenSize;
	dataPtr->primitiveCount = static_cast<uint32_t>(scene->getPrimitiveCount());
	dataPtr->padding = 0.0f;

	m_context->Unmap(m_constantBuffer.Get(), 0);
}

void WorldSpaceUIPass::createLightsBuffer()
{
	D3D11_BUFFER_DESC lightsBufferDesc = {};
	lightsBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	lightsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	lightsBufferDesc.ByteWidth = sizeof(LightData) * MAX_LIGHTS; // Allocate space for up to 100 lights
	lightsBufferDesc.StructureByteStride = sizeof(LightData);

	{
		HRESULT hr = m_device->CreateBuffer(&lightsBufferDesc, nullptr, &m_lightsBuffer);
		if (FAILED(hr))
			std::cerr << "Error Creating Lights Buffer: " << hr << std::endl;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_LIGHTS; // Match the buffer size

	{
		HRESULT hr = m_device->CreateShaderResourceView(m_lightsBuffer.Get(), &srvDesc, &m_lightsSRV);
		if (FAILED(hr))
			std::cerr << "Error Creating Lights SRV: " << hr << std::endl;
	}
}

void WorldSpaceUIPass::updateLights(Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	HRESULT hr = m_context->Map(m_lightsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* lightData = static_cast<LightData*>(mappedResource.pData);
		const auto& lights = scene->getLights();
		int32_t i = 0;
		for (auto& [handle, light] : lights)
		{
			lightData[i] = light->getLightData();
			++i;
		}
		m_context->Unmap(m_lightsBuffer.Get(), 0);
	}
}

void WorldSpaceUIPass::createRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
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
}

void WorldSpaceUIPass::createDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;

	{
		HRESULT hr = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
		if (FAILED(hr))
			std::cerr << "Error Creating Depths Stencil State: " << hr << std::endl;
	}
}

void WorldSpaceUIPass::createQuad()
{
	QuadVertex verts[4] =
	{
		{{-1.f,-1.f}, {0.f,1.f}},
		{{-1.f, 1.f}, {0.f,0.f}},
		{{ 1.f, 1.f}, {1.f,0.f}},
		{{ 1.f,-1.f}, {1.f,1.f}},
	};

	uint16_t idx[6] = { 0,1,2, 0,2,3 };

	D3D11_BUFFER_DESC vbd = {};
	vbd.ByteWidth = sizeof(verts);
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vinit = { verts };
	m_device->CreateBuffer(&vbd, &vinit, m_vertexBuffer.GetAddressOf());

	D3D11_BUFFER_DESC ibd = {};
	ibd.ByteWidth = sizeof(idx);
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA iinit = { idx };
	m_device->CreateBuffer(&ibd, &iinit, m_indexBuffer.GetAddressOf());
}

void WorldSpaceUIPass::draw(const glm::mat4& view,
	const glm::mat4& projection,
	Scene* scene,
	ComPtr<ID3D11RenderTargetView> objectIDRTV)
{

	DEBUG_PASS_START(L"World Space UI Pass Draw");
	if (scene->areLightsDirty())
	{
		updateLights(scene);
	}
	updateConstantBuffer(view, projection, scene);
	if (AppConfig::getNeedsResize())
	{
		createOrResize();
	}
	static const UINT stride = sizeof(QuadVertex);
	static const UINT offset = 0;
	ID3D11ShaderResourceView* srvs[2] = { m_lightIconTexture->srv.Get(), m_lightsSRV.Get() };

	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	m_context->VSSetShader(m_shaderManager->getVertexShader("wordSpaceUIVS"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->VSSetShaderResources(1, 1, m_lightsSRV.GetAddressOf());

	m_context->PSSetShader(m_shaderManager->getPixelShader("wordSpaceUIPS"), nullptr, 0);
	m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->PSSetShaderResources(0, 2, srvs);
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	ID3D11RenderTargetView* rtvs[2] = { m_rtv.Get(), objectIDRTV.Get() };
	m_context->OMSetRenderTargets(2, rtvs, nullptr);
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);

	m_context->DrawIndexedInstanced(6, (UINT)scene->getLights().size(), 0, 0, 0);

	// Reset states to avoid affecting subsequent passes
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
	m_context->PSSetShaderResources(0, 2, nullSRVs);
	m_context->VSSetShaderResources(1, 1, &nullSRV);
	m_context->OMSetRenderTargets(2, nullRTVs, nullptr);
	DEBUG_PASS_END();
}

ComPtr<ID3D11RenderTargetView> WorldSpaceUIPass::getRTV()
{
	return m_rtv;
}

ComPtr<ID3D11ShaderResourceView> WorldSpaceUIPass::getSRV()
{
	return m_srv;
}
