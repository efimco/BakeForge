#include "rayTracePass.hpp"
#include <d3d11.h>
#include <iterator>

#include "appConfig.hpp"
#include "basePass.hpp"
#include "bvhNode.hpp"
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

	glm::mat4 worldMatrix;
	glm::mat4 worldMatrixInv;
};


RayTracePass::RayTracePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadComputeShader("rayTrace", ShaderManager::GetShaderPath(L"rayTrace.hlsl"));
	m_constantBuffer = createConstantBuffer(sizeof(RayTraceConstantBuffer));

	m_bvhNodesBuffer = createStructuredBuffer(sizeof(BVH::Node), 4096 * 4, SBPreset::CpuWrite);
	m_bvhNodesSrv = createShaderResourceView(m_bvhNodesBuffer.Get(), SRVPreset::StructuredBuffer);
}

void RayTracePass::createOrResize()
{
	if (m_texture)
	{
		m_texture.Reset();
		m_srv.Reset();
		m_uav.Reset();
	}

	m_texture = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_srv = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);
	m_uav = createUnorderedAccessView(m_texture.Get(), UAVPreset::Texture2D);
	m_rtvCollector->addRTV("RayTrace", m_srv.Get());
}


void RayTracePass::draw(Scene* scene)
{
	beginDebugEvent(L"RayTrace");

	if (scene->getPrimitiveCount() == 0)
		return;

	const auto& prims = scene->getPrimitives();
	const auto prim = prims.begin()->second;
	if (prim)
	{
		update(scene, prim);
		const auto& triangleBufferSRV = prim->getTrisBufferSRV();
		const auto& triangleIndicesBufferSRV = prim->getTrisIndicesBufferSRV();

		ID3D11ShaderResourceView* srvs[] = { triangleBufferSRV.Get(), triangleIndicesBufferSRV.Get(),
											m_bvhNodesSrv.Get() };
		m_context->CSSetShader(m_shaderManager->getComputeShader("rayTrace"), nullptr, 0);
		m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
		m_context->CSSetShaderResources(0, 3, srvs);
		m_context->CSSetUnorderedAccessViews(0, 1, m_uav.GetAddressOf(), nullptr);
		m_context->Dispatch(AppConfig::viewportWidth / 16, AppConfig::viewportHeight / 16, 1);
		m_context->CSSetShader(nullptr, nullptr, 0);
		unbindShaderResources(0, 3);
		unbindComputeUAVs(0, 1);
	}
	endDebugEvent();
}

void RayTracePass::update(Scene* scene, const Primitive* const& prim)
{
	if (!prim)
		return;
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		RayTraceConstantBuffer* constantBuffer = static_cast<RayTraceConstantBuffer*>(mappedResource.pData);
		constantBuffer->demensions = glm::uvec2(AppConfig::viewportWidth, AppConfig::viewportHeight);

		constantBuffer->camPosition = scene->getActiveCamera()->transform.position;
		constantBuffer->cameraFOV = scene->getActiveCamera()->fov;
		constantBuffer->camForward = scene->getActiveCamera()->front;
		constantBuffer->camRight = scene->getActiveCamera()->right;
		constantBuffer->camUp = scene->getActiveCamera()->up;

		glm::mat4 worldMatrix = const_cast<Primitive*>(prim)->getWorldMatrix();
		constantBuffer->worldMatrix = worldMatrix;
		constantBuffer->worldMatrixInv = glm::inverse(worldMatrix);

		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource1 = {};
	if (SUCCEEDED(m_context->Map(m_bvhNodesBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource1)))
	{
		auto* nodes = static_cast<BVH::Node*>(mappedResource1.pData);
		for (int i = 0; i < prim->getBVHNodes().size(); ++i)
		{
			nodes[i] = prim->getBVHNodes()[i];
		}
		m_context->Unmap(m_bvhNodesBuffer.Get(), 0);
	}
}
