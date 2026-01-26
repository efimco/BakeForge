#include "bakerPass.hpp"

#include <iostream>

#include "glm/glm.hpp"

#include "appConfig.hpp"
#include "primitive.hpp"
#include "primitiveData.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

#define PROFILE_BAKER_PASS

struct alignas(16) BakerCB
{
	glm::mat4 worldMatrix;
	glm::mat4 worldMatrixInvTranspose;
	glm::uvec2 dimensions;
	float cageOffset;
	float padding;
};

struct alignas(16) RaycastVisCB
{
	glm::mat4 viewProjection;
	glm::uvec2 textureDimensions;
	float rayLength;
	float padding;
};


BakerPass::BakerPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_device = device;
	m_context = context;

	m_rtvCollector = std::make_unique<RTVCollector>();

	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_shaderManager->LoadComputeShader("bakerPrepareTextures", L"../../src/shaders/baker.hlsl", "CSPrepareTextures");
	m_shaderManager->LoadComputeShader("bakerBakeNormal", L"../../src/shaders/baker.hlsl", "CSBakeNormal");
	m_shaderManager->LoadVertexShader("raycastDebug", L"../../src/shaders/raycastDebug.hlsl", "VS");
	m_shaderManager->LoadPixelShader("raycastDebug", L"../../src/shaders/raycastDebug.hlsl", "PS");
	m_shaderManager->LoadVertexShader("uvRasterize", L"../../src/shaders/uvRasterize.hlsl", "VS");
	m_shaderManager->LoadPixelShader("uvRasterize", L"../../src/shaders/uvRasterize.hlsl", "PS");

	m_constantBuffer = createConstantBuffer(sizeof(BakerCB));
	m_raycastConstantBuffer = createConstantBuffer(sizeof(RaycastVisCB));

	m_raycastRasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_raycastDepthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);
	m_uvRasterRasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_uvRasterDepthStencilState = createDSState(DepthStencilPreset::Disabled);

	// Create input layout for UV rasterization (matches Vertex struct)
	static constexpr D3D11_INPUT_ELEMENT_DESC uvRasterInputLayoutDesc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	m_device->CreateInputLayout(uvRasterInputLayoutDesc, ARRAYSIZE(uvRasterInputLayoutDesc),
		m_shaderManager->getVertexShaderBlob("uvRasterize")->GetBufferPointer(),
		m_shaderManager->getVertexShaderBlob("uvRasterize")->GetBufferSize(),
		&m_uvRasterInputLayout);

	createOrResize();

	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	m_device->CreateQuery(&queryDesc, &m_disjointQuery);

	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
	m_device->CreateQuery(&queryDesc, &m_startQuery);
	m_device->CreateQuery(&queryDesc, &m_endQuery);

}

void BakerPass::bake(uint32_t width, uint32_t height, float cageOffset)
{
	m_lastWidth = width;
	m_lastHeight = height;
	m_cageOffset = cageOffset;

	m_context->Begin(m_disjointQuery.Get());
	m_context->End(m_startQuery.Get());

	createInterpolatedTextures();
	m_context->End(m_endQuery.Get());
	m_context->End(m_disjointQuery.Get());

#ifdef PROFILE_BAKER_PASS
	// Wait for GPU to finish and get results
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
	while (m_context->GetData(m_disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
	{
		Sleep(1);
	}

	if (!disjointData.Disjoint)
	{
		UINT64 startTime, endTime;
		m_context->GetData(m_startQuery.Get(), &startTime, sizeof(startTime), 0);
		m_context->GetData(m_endQuery.Get(), &endTime, sizeof(endTime), 0);

		double gpuTimeMs = (endTime - startTime) / static_cast<double>(disjointData.Frequency) * 1000.0;
		std::cout << "TOTAL GPU work for baking took " << gpuTimeMs << " ms" << std::endl;
	}
#endif

}

void BakerPass::drawRaycastVisualization(const glm::mat4& view, const glm::mat4& projection)
{
	if (m_lastWidth == 0 || m_lastHeight == 0)
		return;

	const uint32_t numInstances = m_lastHeight * m_lastWidth;
	beginDebugEvent(L"Raycast Visualization");
	updateRaycastVisualization(view, projection);

	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	m_context->ClearRenderTargetView(m_raycastVisualizationRTV.Get(), clearColor);
	m_context->ClearDepthStencilView(m_raycastDepthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	m_context->IASetInputLayout(nullptr);
	m_context->OMSetRenderTargets(1, m_raycastVisualizationRTV.GetAddressOf(), m_raycastDepthDSV.Get());
	m_context->RSSetState(m_raycastRasterizerState.Get());
	m_context->OMSetDepthStencilState(m_raycastDepthStencilState.Get(), 0);
	m_context->VSSetShader(m_shaderManager->getVertexShader("raycastDebug"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("raycastDebug"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_raycastConstantBuffer.GetAddressOf());
	m_context->VSSetShaderResources(0, 1, m_worldSpaceTexelPositionSRV.GetAddressOf());
	m_context->VSSetShaderResources(1, 1, m_worldSpaceTexelNormalSRV.GetAddressOf());
	m_context->DrawInstanced(2, numInstances, 0, 0);

	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	m_context->VSSetShaderResources(0, 2, nullSRVs);
	m_context->RSSetState(nullptr);
	m_context->OMSetRenderTargets(0, nullptr, nullptr);
	endDebugEvent();
}

void BakerPass::createOrResize()
{
	if (m_raycastVisualizationTexture != nullptr)
	{
		m_raycastVisualizationTexture.Reset();
		m_raycastVisualizationSRV.Reset();
		m_raycastVisualizationRTV.Reset();
		m_raycastDepthTexture.Reset();
		m_raycastDepthDSV.Reset();
	}
	m_raycastVisualizationTexture = createTexture2D(AppConfig::windowWidth, AppConfig::windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
	m_raycastVisualizationSRV = createShaderResourceView(m_raycastVisualizationTexture.Get(), SRVPreset::Texture2D);
	m_raycastVisualizationRTV = createRenderTargetView(m_raycastVisualizationTexture.Get(), RTVPreset::Texture2D);

	m_raycastDepthTexture = createTexture2D(AppConfig::windowWidth, AppConfig::windowHeight, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
	m_raycastDepthDSV = createDepthStencilView(m_raycastDepthTexture.Get(), DSVPreset::Texture2D);

	m_rtvCollector->addRTV(name + "::RaycastVisualization", m_raycastVisualizationSRV.Get());
}

void BakerPass::setPrimitivesToBake(const std::vector<std::pair<Primitive*, Primitive*>>& primitivePairs)
{
	m_primitivePairs = primitivePairs;
}


HighPolyPrimitiveBuffers BakerPass::createHighPolyPrimitiveBuffers(Primitive* prim)
{
	const auto& triangleBufferSRV = prim->getTrisBufferSRV();
	const auto& triangleIndicesBufferSRV = prim->getTrisIndicesBufferSRV();

	HighPolyPrimitiveBuffers buffers;
	buffers.srv_triangleBuffer = triangleBufferSRV;
	buffers.srv_triangleIndicesBuffer = triangleIndicesBufferSRV;
	buffers.bvhNodesBuffer = createStructuredBuffer(sizeof(BVH::Node), static_cast<UINT>(prim->getBVHNodes().size()), SBPreset::CpuWrite);
	buffers.bvhNodesSRV = createShaderResourceView(buffers.bvhNodesBuffer.Get(), SRVPreset::StructuredBuffer);
	return buffers;
}

void BakerPass::updateHighPolyPrimitiveBuffers(Primitive* prim, const HighPolyPrimitiveBuffers& buffers)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	if (SUCCEEDED(m_context->Map(buffers.bvhNodesBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		auto* nodes = static_cast<BVH::Node*>(mappedResource.pData);
		for (int i = 0; i < prim->getBVHNodes().size(); ++i)
		{
			nodes[i] = prim->getBVHNodes()[i];
		}
		m_context->Unmap(buffers.bvhNodesBuffer.Get(), 0);
	}
}

void BakerPass::rasterizeUVSpace(Primitive* lowPoly)
{
	// Update constant buffer with world matrix
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<BakerCB*>(mapped.pData);
		data->dimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->worldMatrix = glm::transpose(lowPoly->getWorldMatrix());
		data->worldMatrixInvTranspose = glm::inverse(lowPoly->getWorldMatrix());
		data->cageOffset = m_cageOffset;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	// Clear render targets
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_context->ClearRenderTargetView(m_worldSpaceTexelPositionRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_worldSpaceTexelNormalRTV.Get(), clearColor);

	// Set viewport to texture dimensions
	D3D11_VIEWPORT vp = { 0, 0, static_cast<float>(m_lastWidth), static_cast<float>(m_lastHeight), 0.0f, 1.0f };
	m_context->RSSetViewports(1, &vp);

	// Set render targets (MRT: position + normal)
	ID3D11RenderTargetView* rtvs[2] = { m_worldSpaceTexelPositionRTV.Get(), m_worldSpaceTexelNormalRTV.Get() };
	m_context->OMSetRenderTargets(2, rtvs, nullptr);

	// Set pipeline state
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetInputLayout(m_uvRasterInputLayout.Get());
	m_context->RSSetState(m_uvRasterRasterizerState.Get());
	m_context->OMSetDepthStencilState(m_uvRasterDepthStencilState.Get(), 0);

	// Set shaders
	m_context->VSSetShader(m_shaderManager->getVertexShader("uvRasterize"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("uvRasterize"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	// Set vertex and index buffers from the primitive
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	auto vb = lowPoly->getVertexBuffer();
	auto ib = lowPoly->getIndexBuffer();
	m_context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Draw
	m_context->DrawIndexed(static_cast<UINT>(lowPoly->getIndexData().size()), 0, 0);

	// Unbind render targets
	ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
	m_context->OMSetRenderTargets(2, nullRTVs, nullptr);}



void BakerPass::createInterpolatedTextures()
{
	beginDebugEvent(L"Baker::Interpolated Textures");

#ifdef PROFILE_BAKER_PASS
	m_context->Begin(m_disjointQuery.Get());
	m_context->End(m_startQuery.Get());
#endif

	createInterpolatedTexturesResources();
	createBakedNormalResources();
	for (const auto& [lowPoly, highPoly] : m_primitivePairs)
	{
		if (!lowPoly || !highPoly)
			continue;

		auto hpBuffers = createHighPolyPrimitiveBuffers(highPoly);
		updateHighPolyPrimitiveBuffers(highPoly, hpBuffers);

		rasterizeUVSpace(lowPoly);

#ifdef PROFILE_BAKER_PASS
		m_context->End(m_disjointQuery.Get());

		// Wait for GPU to finish and get results
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
		while (m_context->GetData(m_disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
		{
			Sleep(1);
		}

		if (!disjointData.Disjoint)
		{
			UINT64 startTime, endTime;
			m_context->GetData(m_startQuery.Get(), &startTime, sizeof(startTime), 0);
			m_context->GetData(m_endQuery.Get(), &endTime, sizeof(endTime), 0);

			double gpuTimeMs = (endTime - startTime) / static_cast<double>(disjointData.Frequency) * 1000.0;
			std::cout << "Texture Preparation took " << gpuTimeMs << " ms" << std::endl;
		}
#endif

		bakeNormals(hpBuffers);
	}
	endDebugEvent();
}

void BakerPass::bakeNormals(const HighPolyPrimitiveBuffers& hpBuffers)
{
	beginDebugEvent(L"Baker::Bake Normals");

#ifdef PROFILE_BAKER_PASS
	m_context->Begin(m_disjointQuery.Get());
	m_context->End(m_startQuery.Get());
#endif

	m_context->CSSetShader(m_shaderManager->getComputeShader("bakerBakeNormal"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	ID3D11UnorderedAccessView* bakedNormalUAVs[1] = { m_bakedNormalUAV.Get() };
	m_context->CSSetUnorderedAccessViews(2, 1, bakedNormalUAVs, nullptr);
	ID3D11ShaderResourceView* hpSRVs[5] = {
		hpBuffers.srv_triangleBuffer.Get(),
		hpBuffers.srv_triangleIndicesBuffer.Get(),
		hpBuffers.bvhNodesSRV.Get(),
		m_worldSpaceTexelPositionSRV.Get(),
		m_worldSpaceTexelNormalSRV.Get()
	};
	m_context->CSSetShaderResources(2, 5, hpSRVs);
	UINT threadGroupX = (m_lastWidth + 15) / 16;
	UINT threadGroupY = (m_lastHeight + 15) / 16;
	m_context->Dispatch(threadGroupX, threadGroupY, 1);
	unbindComputeUAVs(2, 1);
	unbindShaderResources(2, 5);

	m_context->End(m_endQuery.Get());
	m_context->End(m_disjointQuery.Get());

#ifdef PROFILE_BAKER_PASS
	// Wait for GPU to finish and get results
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
	while (m_context->GetData(m_disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
	{
		Sleep(1);
	}

	if (!disjointData.Disjoint)
	{
		UINT64 startTime, endTime;
		m_context->GetData(m_startQuery.Get(), &startTime, sizeof(startTime), 0);
		m_context->GetData(m_endQuery.Get(), &endTime, sizeof(endTime), 0);

		double gpuTimeMs = (endTime - startTime) / static_cast<double>(disjointData.Frequency) * 1000.0;
		std::cout << "GPU baking took " << gpuTimeMs << " ms" << std::endl;
	}
#endif
	endDebugEvent();
}


void BakerPass::updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_raycastConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<RaycastVisCB*>(mapped.pData);
		data->viewProjection = glm::transpose(projection * view);
		data->textureDimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->rayLength = 0.1f;
		m_context->Unmap(m_raycastConstantBuffer.Get(), 0);
	}
}

void BakerPass::createInterpolatedTexturesResources()
{
	if (m_worldSpaceTexelPositionTexture != nullptr)
	{
		m_worldSpaceTexelPositionTexture.Reset();
		m_worldSpaceTexelPositionSRV.Reset();
		m_worldSpaceTexelPositionUAV.Reset();
		m_worldSpaceTexelPositionRTV.Reset();

		m_worldSpaceTexelNormalTexture.Reset();
		m_worldSpaceTexelNormalSRV.Reset();
		m_worldSpaceTexelNormalUAV.Reset();
		m_worldSpaceTexelNormalRTV.Reset();
	}

	m_worldSpaceTexelPositionTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_worldSpaceTexelPositionSRV = createShaderResourceView(m_worldSpaceTexelPositionTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelPositionUAV = createUnorderedAccessView(m_worldSpaceTexelPositionTexture.Get(), UAVPreset::Texture2D, 0);
	m_worldSpaceTexelPositionRTV = createRenderTargetView(m_worldSpaceTexelPositionTexture.Get(), RTVPreset::Texture2D);

	m_worldSpaceTexelNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_worldSpaceTexelNormalSRV = createShaderResourceView(m_worldSpaceTexelNormalTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelNormalUAV = createUnorderedAccessView(m_worldSpaceTexelNormalTexture.Get(), UAVPreset::Texture2D, 0);
	m_worldSpaceTexelNormalRTV = createRenderTargetView(m_worldSpaceTexelNormalTexture.Get(), RTVPreset::Texture2D);

	m_rtvCollector->addRTV(name + "::WorldSpaceTexelPosition", m_worldSpaceTexelPositionSRV.Get());
	m_rtvCollector->addRTV(name + "::WorldSpaceTexelNormal", m_worldSpaceTexelNormalSRV.Get());
}

void BakerPass::createBakedNormalResources()
{
	if (m_bakedNormalTexture != nullptr)
	{
		m_bakedNormalTexture.Reset();
		m_bakedNormalSRV.Reset();
		m_bakedNormalUAV.Reset();
	}

	m_bakedNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_bakedNormalSRV = createShaderResourceView(m_bakedNormalTexture.Get(), SRVPreset::Texture2D);
	m_bakedNormalUAV = createUnorderedAccessView(m_bakedNormalTexture.Get(), UAVPreset::Texture2D, 0);

	m_rtvCollector->addRTV(name + "::BakedNormalTexture", m_bakedNormalSRV.Get());
}

