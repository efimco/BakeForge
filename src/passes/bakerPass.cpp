#include "bakerPass.hpp"
#include "glm/glm.hpp"

#include "appConfig.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

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

	m_constantBuffer = createConstantBuffer(sizeof(BakerCB));
	m_raycastConstantBuffer = createConstantBuffer(sizeof(RaycastVisCB));

	m_raycastRasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_raycastDepthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);
	createOrResize();
}

void BakerPass::bake(uint32_t width, uint32_t height, float cageOffset)
{
	m_lastWidth = width;
	m_lastHeight = height;
	m_cageOffset = cageOffset;
	createInterpolatedTextures();

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

LowPolyPrimitiveBuffers BakerPass::createLowPolyPrimitiveBuffers(Primitive* prim)
{
	LowPolyPrimitiveBuffers buffers;
	buffers.structuredIndexBuffer = createStructuredBuffer(sizeof(uint32_t), static_cast<UINT>(prim->getIndexData().size()), SBPreset::CpuWrite);
	buffers.structuredVertexBuffer = createStructuredBuffer(sizeof(Vertex), static_cast<UINT>(prim->getVertexData().size()), SBPreset::CpuWrite);
	buffers.structuredIndexSRV = createShaderResourceView(buffers.structuredIndexBuffer.Get(), SRVPreset::StructuredBuffer);
	buffers.structuredVertexSRV = createShaderResourceView(buffers.structuredVertexBuffer.Get(), SRVPreset::StructuredBuffer);
	return buffers;
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

void BakerPass::updateLowPolyPrimitiveBuffers(Primitive* prim, const LowPolyPrimitiveBuffers& buffers)
{
	D3D11_MAPPED_SUBRESOURCE mappedVertexBuffer = {};
	if (SUCCEEDED(m_context->Map(buffers.structuredVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertexBuffer)))
	{
		auto* verticies = static_cast<Vertex*>(mappedVertexBuffer.pData);
		const auto& primVertices = prim->getVertexData();
		for (size_t i = 0; i < primVertices.size(); ++i)
		{
			verticies[i] = primVertices[i];
		}
		m_context->Unmap(buffers.structuredVertexBuffer.Get(), 0);
	}

	D3D11_MAPPED_SUBRESOURCE mappedIndexBuffer = {};
	if (SUCCEEDED(m_context->Map(buffers.structuredIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndexBuffer)))
	{
		auto* indices = static_cast<uint32_t*>(mappedIndexBuffer.pData);
		const auto& primIndices = prim->getIndexData();
		for (size_t i = 0; i < primIndices.size(); ++i)
		{
			indices[i] = primIndices[i];
		}
		m_context->Unmap(buffers.structuredIndexBuffer.Get(), 0);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<BakerCB*>(mapped.pData);
		data->dimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->worldMatrix = glm::transpose(prim->getWorldMatrix());
		data->worldMatrixInvTranspose = glm::inverse(prim->getWorldMatrix());
		data->cageOffset = m_cageOffset;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	};
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

void BakerPass::createInterpolatedTextures()
{
	beginDebugEvent(L"Baker::Interpolated Textures");
	createInterpolatedTexturesResources();
	createBakedNormalResources();
	for (const auto& [lowPoly, highPoly] : m_primitivePairs)
	{
		if (!lowPoly || !highPoly)
			continue;

		auto lpBuffers = createLowPolyPrimitiveBuffers(lowPoly);
		auto hpBuffers = createHighPolyPrimitiveBuffers(highPoly);

		updateLowPolyPrimitiveBuffers(lowPoly, lpBuffers);
		updateHighPolyPrimitiveBuffers(highPoly, hpBuffers);


		m_context->CSSetShader(m_shaderManager->getComputeShader("bakerPrepareTextures"), nullptr, 0);
		m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

		ID3D11UnorderedAccessView* interpolatedTexturesUAVs[2] = { m_worldSpaceTexelPositionUAV.Get(),m_worldSpaceTexelNormalUAV.Get() };
		m_context->CSSetUnorderedAccessViews(0, 2, interpolatedTexturesUAVs, nullptr);

		ID3D11ShaderResourceView* lpSRVs[2] = { lpBuffers.structuredVertexSRV.Get(), lpBuffers.structuredIndexSRV.Get() };
		m_context->CSSetShaderResources(0, 2, lpSRVs);

		UINT threadGroupX = (m_lastWidth + 15) / 16;
		UINT threadGroupY = (m_lastHeight + 15) / 16;
		m_context->Dispatch(threadGroupX, threadGroupY, 1);
		unbindComputeUAVs(0, 2);
		unbindShaderResources(0, 2);

		bakeNormals(hpBuffers);
	}
	endDebugEvent();
}

void BakerPass::bakeNormals(const HighPolyPrimitiveBuffers& hpBuffers)
{
	beginDebugEvent(L"Baker::Bake Normals");
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

		m_worldSpaceTexelNormalTexture.Reset();
		m_worldSpaceTexelNormalSRV.Reset();
		m_worldSpaceTexelNormalUAV.Reset();
	}

	m_worldSpaceTexelPositionTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_worldSpaceTexelPositionSRV = createShaderResourceView(m_worldSpaceTexelPositionTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelPositionUAV = createUnorderedAccessView(m_worldSpaceTexelPositionTexture.Get(), UAVPreset::Texture2D, 0);

	m_worldSpaceTexelNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_worldSpaceTexelNormalSRV = createShaderResourceView(m_worldSpaceTexelNormalTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelNormalUAV = createUnorderedAccessView(m_worldSpaceTexelNormalTexture.Get(), UAVPreset::Texture2D, 0);

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

