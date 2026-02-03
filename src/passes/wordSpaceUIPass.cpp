#include "wordSpaceUIPass.hpp"

#include <memory>

#include "appConfig.hpp"
#include "basePass.hpp"
#include "light.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"
#include "texture.hpp"

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
	glm::vec2 padding;
};

static const QuadVertex verts[4] = {
	{{-1.f, -1.f}, {0.f, 1.f}},
	{{-1.f, 1.f}, {0.f, 0.f}},
	{{1.f, 1.f}, {1.f, 0.f}},
	{{1.f, -1.f}, {1.f, 1.f}},
};

static const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};


static constexpr D3D11_INPUT_ELEMENT_DESC s_WSUIInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


WorldSpaceUIPass::WorldSpaceUIPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_lightIconTexture = std::make_shared<Texture>(AppConfig::GetResourcePathA("icons/PointLight.png"), m_device);
	m_shaderManager->LoadPixelShader("wordSpaceUIPS", ShaderManager::GetShaderPath(L"wordSpaceUI.hlsl"), "PS");
	m_shaderManager->LoadVertexShader("wordSpaceUIVS", ShaderManager::GetShaderPath(L"wordSpaceUI.hlsl"), "VS");
	m_vertexBuffer = createVertexBuffer(sizeof(verts), verts);
	m_indexBuffer = createIndexBuffer(sizeof(idx), idx);
	createOrResize();
	m_lightsBuffer = createStructuredBuffer(sizeof(LightData), MAX_LIGHTS, SBPreset::CpuWrite);
	m_lightsSRV = createShaderResourceView(m_lightsBuffer.Get(), SRVPreset::StructuredBuffer);
	createInputLayout();
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);
	m_samplerState = createSamplerState(SamplerPreset::LinearClamp);
	m_constantBuffer = createConstantBuffer(sizeof(ConstantBufferData));
}

void WorldSpaceUIPass::createOrResize()
{
	if (m_texture != nullptr)
	{
		m_texture.Reset();
		m_rtv.Reset();
		m_srv.Reset();
	}

	m_texture =
		createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_srv = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);
	m_rtv = createRenderTargetView(m_texture.Get(), RTVPreset::Texture2D);
	m_rtvCollector->addRTV("WSUI::wsuiTextrue", m_srv.Get());
}

void WorldSpaceUIPass::createInputLayout()
{
	HRESULT hr = m_device->CreateInputLayout(s_WSUIInputLayoutDesc, ARRAYSIZE(s_WSUIInputLayoutDesc),
											 m_shaderManager->getVertexShaderBlob("wordSpaceUIVS")->GetBufferPointer(),
											 m_shaderManager->getVertexShaderBlob("wordSpaceUIVS")->GetBufferSize(),
											 &m_inputLayout);
	assert(SUCCEEDED(hr));
}


void WorldSpaceUIPass::updateConstantBuffer(const glm::mat4& view, const glm::mat4& projection, Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(SUCCEEDED(hr));

	ConstantBufferData* dataPtr = (ConstantBufferData*)mappedResource.pData;
	glm::mat4 vp = projection * view;
	dataPtr->viewProjection = glm::transpose(vp);
	dataPtr->view = glm::transpose(view);
	dataPtr->cameraPosition = glm::vec3(scene->transform.position);
	dataPtr->sizeInPixels = 32.0f; // Example value, adjust as needed
	glm::ivec2 screenSize = glm::ivec2(AppConfig::viewportWidth, AppConfig::viewportHeight);
	dataPtr->screenSize = screenSize;

	m_context->Unmap(m_constantBuffer.Get(), 0);
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
			lightData[i].objectID = static_cast<float>(static_cast<int32_t>(handle));
			++i;
		}
		m_context->Unmap(m_lightsBuffer.Get(), 0);
	}
}

void WorldSpaceUIPass::draw(const glm::mat4& view,
							const glm::mat4& projection,
							Scene* scene,
							ComPtr<ID3D11RenderTargetView> objectIDRTV)
{

	beginDebugEvent(L"WorldSpaceUIPass");
	if (scene->areLightsDirty())
	{
		updateLights(scene);
	}
	updateConstantBuffer(view, projection, scene);
	static const UINT stride = sizeof(QuadVertex);
	static const UINT offset = 0;
	ID3D11ShaderResourceView* srvs[2] = {m_lightIconTexture->srv.Get(), m_lightsSRV.Get()};
	setViewport(AppConfig::viewportWidth, AppConfig::viewportHeight);
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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

	float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
	ID3D11RenderTargetView* rtvs[2] = {m_rtv.Get(), objectIDRTV.Get()};
	m_context->OMSetRenderTargets(2, rtvs, nullptr);

	m_context->DrawIndexedInstanced(6, (UINT)scene->getLights().size(), 0, 0, 0);

	// Reset states to avoid affecting subsequent passes
	ID3D11ShaderResourceView* nullSRVs[2] = {nullptr, nullptr};
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11RenderTargetView* nullRTVs[2] = {nullptr, nullptr};
	m_context->PSSetShaderResources(0, 2, nullSRVs);
	m_context->VSSetShaderResources(1, 1, &nullSRV);
	m_context->OMSetRenderTargets(2, nullRTVs, nullptr);
	endDebugEvent();
}

ComPtr<ID3D11RenderTargetView> WorldSpaceUIPass::getRTV()
{
	return m_rtv;
}

ComPtr<ID3D11ShaderResourceView> WorldSpaceUIPass::getSRV()
{
	return m_srv;
}
