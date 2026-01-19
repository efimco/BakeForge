#include "rayTracePass.hpp"

#include "appConfig.hpp"
#include "basePass.hpp"
#include "camera.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

struct alignas(16) RayTraceConstantBuffer
{
	glm::uvec2 demensions;
	glm::vec2 padding = glm::vec2(0, 0);

	glm::vec3 camPosition;
	float padding2 = 0;

	glm::vec3 camForward;
	float padding3 = 0;

	glm::vec3 camRight;
	float padding4 = 0;

	glm::vec3 camUp;

	float cameraFOV;
};


RayTracePass::RayTracePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
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
								DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_srv = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);
	m_uav = createUnorderedAccessView(m_texture.Get(), UAVPreset::Texture2D);
	m_rtvCollector->addRTV("RayTrace", m_srv.Get());
}


void RayTracePass::draw(Scene* scene)
{
	beginDebugEvent(L"RayTrace");
	update(scene);

	if (scene->getPrimitiveCount() == 0)
		return;

	for (const auto& [handle, prim] : scene->getPrimitives())
	{
		auto vertexBufferSRV = prim->getVertexStructuredBufferSRV();
		auto indexBufferSRV = prim->getIndexStructuredBufferSRV();
		ID3D11ShaderResourceView* srvs[] = {vertexBufferSRV.Get(), indexBufferSRV.Get()};
		m_context->CSSetShader(m_shaderManager->getComputeShader("rayTrace"), nullptr, 0);
		m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
		m_context->CSSetShaderResources(0, 2, srvs);
		m_context->CSSetUnorderedAccessViews(0, 1, m_uav.GetAddressOf(), nullptr);
		m_context->Dispatch(AppConfig::getViewportWidth() / 16, AppConfig::getViewportHeight() / 16, 1);
		m_context->CSSetShader(nullptr, nullptr, 0);
		unbindShaderResources(0, 2);
		unbindComputeUAVs(0, 1);
	}
	endDebugEvent();
}

void RayTracePass::update(Scene* scene)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		RayTraceConstantBuffer* constantBuffer = static_cast<RayTraceConstantBuffer*>(mappedResource.pData);
		constantBuffer->demensions = glm::uvec2(AppConfig::getViewportWidth(), AppConfig::getViewportHeight());

		constantBuffer->camPosition = scene->getActiveCamera()->transform.position;
		constantBuffer->cameraFOV = scene->getActiveCamera()->fov;
		constantBuffer->camForward = scene->getActiveCamera()->front;
		constantBuffer->camRight = scene->getActiveCamera()->right;
		constantBuffer->camUp = scene->getActiveCamera()->up;

		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}
