#include "GBuffer.hpp"

#include <dxgiformat.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <winbase.h>

#include "appConfig.hpp"

#include "GBufferTextures.hpp"
#include "basePass.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

static constexpr D3D11_INPUT_ELEMENT_DESC s_gBufferInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}};

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
	glm::mat4 inverseTransposedModel;
	glm::mat4 model;
	float objectID;
	float padding[3]; // Align to 16 bytes
	glm::vec3 cameraPosition;
	float padding2; // Align to 16 bytes
};

GBuffer::GBuffer(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{
	m_device = device;
	m_context = context;
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::ReadOnlyEqual);
	m_samplerState = createSamplerState(SamplerPreset::AnisotropicWrap);

	// Load shaders via inherited m_shaderManager
	m_shaderManager->LoadPixelShader("gBuffer", L"../../src/shaders/gBuffer.hlsl", "PS");
	m_shaderManager->LoadVertexShader("gBuffer", L"../../src/shaders/gBuffer.hlsl", "VS");

	createOrResize();

	{
		HRESULT hr = m_device->CreateInputLayout(s_gBufferInputLayoutDesc, ARRAYSIZE(s_gBufferInputLayoutDesc),
												 m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferPointer(),
												 m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferSize(),
												 &m_inputLayout);
		assert(SUCCEEDED(hr));
	};
	m_constantbuffer = createConstantBuffer(sizeof(ConstantBufferData));
}

void GBuffer::draw(const glm::mat4& view,
				   const glm::mat4& projection,
				   const glm::vec3& cameraPosition,
				   Scene* scene,
				   ComPtr<ID3D11DepthStencilView> dsv)
{
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(AppConfig::getViewportWidth());
	viewport.Height = static_cast<float>(AppConfig::getViewportHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_context->RSSetViewports(1, &viewport);

	beginDebugEvent(L"GBuffer Pass");
	if (AppConfig::getNeedsResize())
	{
		createOrResize();
	}
	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(5, m_rtvs, dsv.Get());

	for (const auto rtv : m_rtvs)
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

	static const UINT stride = sizeof(Vertex);
	static const UINT offset = 0;
	for (auto& [handle, prim] : scene->getPrimitives())
	{
		const float objectID = static_cast<float>(static_cast<int32_t>(handle));
		if (!prim->material)
		{
			std::cerr << "Primitive " << objectID << " has no material!" << std::endl;
			continue;
		}
		update(view, projection, cameraPosition, scene, objectID, prim);
		m_context->IASetVertexBuffers(0, 1, prim->getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim->getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		ID3D11ShaderResourceView* const* SRVs = prim->material->getSRVs();
		m_context->PSSetShaderResources(0, 3, SRVs);
		m_context->DrawIndexed(static_cast<UINT>(prim->getIndexData().size()), 0, 0);
		unbindShaderResources(0, 3);
	}
	unbindRenderTargets(5);
	endDebugEvent();
}

void GBuffer::update(const glm::mat4& view,
					 const glm::mat4& projection,
					 const glm::vec3& cameraPosition,
					 Scene* scene,
					 const float objectID,
					 Primitive* prim) const
{
	const glm::mat4 model = prim->getWorldMatrix();
	const glm::mat4 mvp = projection * view * model;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		const auto cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		// Use inverse-transpose of the model matrix (upper-left 3x3) for correct normal transformation
		cbData->inverseTransposedModel = glm::inverse(model);
		cbData->model = glm::transpose(model);
		cbData->objectID = objectID;
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

	// albedo
	t_albedo = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R8G8B8A8_UNORM);
	srv_albedo = createShaderResourceView(t_albedo.Get(), SRVPreset::Texture2D);
	rtv_albedo = createRenderTargetView(t_albedo.Get(), RTVPreset::Texture2D);

	// metallicRoughness

	t_metallicRoughness = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R16G16_UNORM);
	srv_metallicRoughness = createShaderResourceView(t_metallicRoughness.Get(), SRVPreset::Texture2D);
	rtv_metallicRoughness = createRenderTargetView(t_metallicRoughness.Get(), RTVPreset::Texture2D);

	// normal

	t_normal = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R10G10B10A2_UNORM);
	srv_normal = createShaderResourceView(t_normal.Get(), SRVPreset::Texture2D);
	rtv_normal = createRenderTargetView(t_normal.Get(), RTVPreset::Texture2D);

	// position

	t_position = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R16G16B16A16_FLOAT);
	srv_position = createShaderResourceView(t_position.Get(), SRVPreset::Texture2D);
	rtv_position = createRenderTargetView(t_position.Get(), RTVPreset::Texture2D);

	// objectID
	t_objectID = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R32_FLOAT);
	srv_objectID = createShaderResourceView(t_objectID.Get(), SRVPreset::Texture2D);
	rtv_objectID = createRenderTargetView(t_objectID.Get(), RTVPreset::Texture2D);

	// rtv assignment
	m_rtvs[0] = rtv_albedo.Get();
	m_rtvs[1] = rtv_metallicRoughness.Get();
	m_rtvs[2] = rtv_normal.Get();
	m_rtvs[3] = rtv_position.Get();
	m_rtvs[4] = rtv_objectID.Get();

	m_rtvCollector->addRTV("GBuffer::Albedo", srv_albedo.Get());
	m_rtvCollector->addRTV("GBuffer::MetallicRoughness", srv_metallicRoughness.Get());
	m_rtvCollector->addRTV("GBuffer::Normal", srv_normal.Get());
	m_rtvCollector->addRTV("GBuffer::Position", srv_position.Get());
	m_rtvCollector->addRTV("GBuffer::ObjectID", srv_objectID.Get());
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

ComPtr<ID3D11ShaderResourceView> GBuffer::getAlbedoSRV() const
{
	return srv_albedo;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getMetallicRoughnessSRV() const
{
	return srv_metallicRoughness;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getNormalSRV() const
{
	return srv_normal;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getPositionSRV() const
{
	return srv_position;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getObjectIDSRV() const
{
	return srv_objectID;
}

ComPtr<ID3D11RenderTargetView> GBuffer::getObjectIDRTV() const
{
	return rtv_objectID;
}
