#pragma once
#include "bvhDebugPass.hpp"

#include <d3d11.h>
#include <winnt.h>

#include "appConfig.hpp"
#include "basePass.hpp"
#include "bvhNode.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shadermanager.hpp"


static const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


static const std::vector<Position> s_vertices = {
	{-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f},
	{1.0f, -1.0f, -1.0f},  {1.0f, -1.0f, 1.0f},	 {1.0f, 1.0f, -1.0f},  {1.0f, 1.0f, 1.0f},
};
//// clang-format off
static const std::vector<uint32_t> s_indices = {
	0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 0, 4, 1, 1, 5, 5, 2, 6, 3, 3, 7, 7, 0, 2, 4, 4, 6, 6, 1, 3, 5, 5, 7, 7,
};

struct alignas(16) BVHCB
{
	glm::mat4 viewProj;
};

BVHDebugPass::BVHDebugPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{

	m_rtvCollector = std::make_unique<RTVCollector>();

	m_shaderManager->LoadVertexShader("debugBVH", L"../../src/shaders/debugBVH.hlsl", "VS");
	m_shaderManager->LoadPixelShader("debugBVH", L"../../src/shaders/debugBVH.hlsl", "PS");

	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_blendState = createBlendState(BlendPreset::AlphaBlend);
	m_depthStencilState = createDSState(DepthStencilPreset::WriteDepth);

	m_constantBuffer = createConstantBuffer(sizeof(BVHCB));

	m_vertexBuffer = createVertexBuffer(static_cast<uint32_t>(sizeof(Position) * s_vertices.size()), s_vertices.data());
	m_indexBuffer = createIndexBuffer(static_cast<uint32_t>(sizeof(uint32_t) * s_indices.size()), s_indices.data());

	m_bvhNodesBuffer = createStructuredBuffer(sizeof(BVH::Node), 4096, SBPreset::CpuWrite);
	m_bvhNodesSrv = createShaderResourceView(m_bvhNodesBuffer.Get(), SRVPreset::StructuredBuffer);

	createOrResize();

	HRESULT hr =
		m_device->CreateInputLayout(inputLayoutDesc, ARRAYSIZE(inputLayoutDesc),
									m_shaderManager->getVertexShaderBlob("debugBVH")->GetBufferPointer(),
									m_shaderManager->getVertexShaderBlob("debugBVH")->GetBufferSize(), &m_inputLayout);
	assert(SUCCEEDED(hr));
}

uint32_t stride = sizeof(Position);
uint32_t offset = 0;

void BVHDebugPass::draw(Scene* scene, const glm::mat4& view, const glm::mat4& proj)
{

	beginDebugEvent(L"DebugBVH");
	update(view, proj);
	// CRITICAL: Clear first!
	float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f}; // Dark gray to see if it's working
	m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->VSSetShader(m_shaderManager->getVertexShader("debugBVH"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("debugBVH"), nullptr, 0);
	m_context->RSSetState(m_rasterizerState.Get());
	m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xFFFFFFFF);
	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	for (auto& [handle, prim] : scene->getPrimitives())
	{
		updateBvhNodesBuffer(prim);
		m_context->VSSetShaderResources(0, 1, m_bvhNodesSrv.GetAddressOf());

		uint32_t numNodes = static_cast<uint32_t>(prim->getBVHNodes().size());
		m_context->DrawIndexedInstanced(static_cast<uint32_t>(s_indices.size()), // index count per instance
										numNodes,								 // instance count
										0,										 // start index location
										0,										 // base vertex location
										0);										 // start instance location
	}
	unbindRenderTargets(1);
	unbindShaderResources(0, 1);
	m_context->IASetInputLayout(nullptr);
	m_context->RSSetState(nullptr);
	m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	m_context->OMSetDepthStencilState(nullptr, 0);
	m_context->VSSetConstantBuffers(0, 0, nullptr);

	endDebugEvent();
}

void BVHDebugPass::update(const glm::mat4& view, const glm::mat4& proj)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* cb = static_cast<BVHCB*>(mappedResource.pData); // Removed const qualifier to allow assignment
		const glm::mat4 viewProj = glm::transpose(proj * view);
		cb->viewProj = viewProj;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}

void BVHDebugPass::updateBvhNodesBuffer(Primitive* prim)
{

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_bvhNodesBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* nodes = static_cast<BVH::Node*>(mappedResource.pData);
		for (int i = 0; i < prim->getBVHNodes().size(); ++i)
		{
			nodes[i] = prim->getBVHNodes()[i];
		}
		m_context->Unmap(m_bvhNodesBuffer.Get(), 0);
	}
}

void BVHDebugPass::createOrResize()
{
	if (m_texture)
	{
		m_depthTexture.Reset();
		m_texture.Reset();
		m_shaderResourceView.Reset();
		m_renderTargetView.Reset();
	}

	m_depthTexture =
		createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R24G8_TYPELESS);
	m_depthStencilView = createDSView(m_depthTexture.Get(), DXGI_FORMAT_D24_UNORM_S8_UINT);

	m_texture =
		createTexture2D(AppConfig::getViewportWidth(), AppConfig::getViewportHeight(), DXGI_FORMAT_R8G8B8A8_UNORM);
	m_shaderResourceView = createShaderResourceView(m_texture.Get(), SRVPreset::Texture2D);
	m_renderTargetView = createRenderTargetView(m_texture.Get(), RTVPreset::Texture2D);

	m_rtvCollector->addRTV("DebugBVH", m_shaderResourceView.Get());
}
