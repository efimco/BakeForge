#include "rayTracePass.hpp"

#include "appConfig.hpp"
#include "basePass.hpp"
#include "camera.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

struct alignas(16) RayTraceConstantBuffer
{
	glm::uvec2 demensions;
	glm::vec2 padding;
	glm::vec3 cameraPosition;
	float padding2;
};

RayTracePass::RayTracePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadComputeShader("rayTrace", L"..\\..\\src\\shaders\\rayTrace.hlsl");
	m_constantBuffer = createConstantBuffer(sizeof(RayTraceConstantBuffer));
}

void RayTracePass::createOrResize()
{
	if (m_texture)
	{
		m_texture.Reset();
		m_srv.Reset();
		m_uav.Reset();
	}

	m_texture = createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(),
								DXGI_FORMAT_R8G8B8A8_UNORM,
								D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_srv = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);
	m_uav = createUnorderedAccessView(m_texture.Get(), UAVPreset::Texture2D);
	m_rtvCollector->addRTV("RayTrace", m_srv.Get());
}


void RayTracePass::draw(Scene* scene)
{
	update(scene);

	if (scene->getPrimitiveCount() == 0)
		return;
	if (!scene->getActiveNode())
		return;
	auto prim = dynamic_cast<Primitive*>(scene->getActiveNode());
	if (!prim)
		return;
	auto trianglesBufferSRV = prim->getTrianglesStructuredBufferSRV();
	m_context->CSSetShader(m_shaderManager->getComputeShader("rayTrace"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->CSSetShaderResources(0, 1, trianglesBufferSRV.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, m_uav.GetAddressOf(), nullptr);
	m_context->Dispatch(AppConfig::getViewportWidth() / 8, AppConfig::getViewportHeight() / 8, 1);
	m_context->CSSetShader(nullptr, nullptr, 0);
	unbindShaderResources(0, 1);
	unbindComputeUAVs(0, 1);
}

void RayTracePass::update(Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		RayTraceConstantBuffer* constantBuffer = static_cast<RayTraceConstantBuffer*>(mappedResource.pData);
		constantBuffer->demensions = glm::uvec2(AppConfig::getViewportWidth(), AppConfig::getViewportHeight());
		constantBuffer->padding = glm::vec2(0.0f);
		constantBuffer->cameraPosition = scene->getActiveCamera()->transform.position;
		constantBuffer->padding2 = 0.0f;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}
