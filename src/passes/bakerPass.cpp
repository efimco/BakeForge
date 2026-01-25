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
	float padding[2];
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
	m_shaderManager->LoadComputeShader("baker", L"../../src/shaders/baker.hlsl", "CS");
	m_shaderManager->LoadVertexShader("raycastDebug", L"../../src/shaders/raycastDebug.hlsl", "VS");
	m_shaderManager->LoadPixelShader("raycastDebug", L"../../src/shaders/raycastDebug.hlsl", "PS");

	m_constantBuffer = createConstantBuffer(sizeof(BakerCB));
	m_raycastConstantBuffer = createConstantBuffer(sizeof(RaycastVisCB));

	m_raycastRasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_raycastDepthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);
	createOrResizeBakedResources(1, 1, nullptr);
	createOrResize();
}

void BakerPass::bake(Scene* scene, uint32_t width, uint32_t height)
{
	m_lastWidth = width;
	m_lastHeight = height;
	const auto& prim = dynamic_cast<Primitive*>(scene->getActiveNode());
	if (!prim)
		return;
	if (!AppConfig::bake)
		return;
	beginDebugEvent(L"Baker Pass");
	createOrResizeBakedResources(width, height, prim);
	updateBake(width, height, prim);
	m_context->CSSetShader(m_shaderManager->getComputeShader("baker"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, m_worldSpaceTexelPositionUAV.GetAddressOf(), nullptr);
	m_context->CSSetUnorderedAccessViews(1, 1, m_worldSpaceTexelNormalUAV.GetAddressOf(), nullptr);
	m_context->CSSetShaderResources(0, 1, m_structuredVertexSRV.GetAddressOf());
	m_context->CSSetShaderResources(1, 1, m_structuredIndexSRV.GetAddressOf());
	UINT threadGroupX = (width + 15) / 16;
	UINT threadGroupY = (height + 15) / 16;
	m_context->Dispatch(threadGroupX, threadGroupY, 1);
	unbindComputeUAVs(0, 2);
	unbindShaderResources(0, 2);
	AppConfig::bake = false;
	endDebugEvent();
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

	m_rtvCollector->addRTV("RaycastVisualization", m_raycastVisualizationSRV.Get());
}

void BakerPass::setPrimitivesToBake(const std::vector<Primitive*>& lowPolyPrims, const std::vector<Primitive*>& highPolyPrims)
{
	m_lowPolyPrimitivesToBake = lowPolyPrims;
	m_highPolyPrimitivesToBake = highPolyPrims;
}

void BakerPass::updateBake(uint32_t width, uint32_t height, Primitive* prim)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<BakerCB*>(mapped.pData);
		data->dimensions = glm::uvec2(width, height);
		data->worldMatrix = glm::transpose(prim->getWorldMatrix());
		data->worldMatrixInvTranspose = glm::inverse(prim->getWorldMatrix());
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	D3D11_MAPPED_SUBRESOURCE mappedVertexBuffer = {};
	if (SUCCEEDED(m_context->Map(m_structuredVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertexBuffer)))
	{
		auto* verticies = static_cast<Vertex*>(mappedVertexBuffer.pData);
		const auto& primVertices = prim->getVertexData();
		for (size_t i = 0; i < primVertices.size(); ++i)
		{
			verticies[i] = primVertices[i];
		}
		m_context->Unmap(m_structuredVertexBuffer.Get(), 0);
	}

	D3D11_MAPPED_SUBRESOURCE mappedIndexBuffer = {};
	if (SUCCEEDED(m_context->Map(m_structuredIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndexBuffer)))
	{
		auto* indices = static_cast<uint32_t*>(mappedIndexBuffer.pData);
		const auto& primIndices = prim->getIndexData();
		for (size_t i = 0; i < primIndices.size(); ++i)
		{
			indices[i] = primIndices[i];
		}
		m_context->Unmap(m_structuredIndexBuffer.Get(), 0);
	}
}

void BakerPass::updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_raycastConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<RaycastVisCB*>(mapped.pData);
		data->viewProjection = glm::transpose(projection * view);
		data->textureDimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->rayLength = AppConfig::rayLength;
		m_context->Unmap(m_raycastConstantBuffer.Get(), 0);
	}
}

void BakerPass::createOrResizeBakedResources(uint32_t width, uint32_t height, Primitive* prim)
{
	if (m_worldSpaceTexelPositionTexture != nullptr)
	{
		m_worldSpaceTexelPositionTexture.Reset();
		m_worldSpaceTexelPositionSRV.Reset();
		m_worldSpaceTexelPositionUAV.Reset();

		m_worldSpaceTexelNormalTexture.Reset();
		m_worldSpaceTexelNormalSRV.Reset();
		m_worldSpaceTexelNormalUAV.Reset();

		m_structuredVertexBuffer.Reset();
		m_structuredVertexSRV.Reset();

		m_structuredIndexBuffer.Reset();
		m_structuredIndexSRV.Reset();

		m_bvhNodesBuffer.Reset();
		m_bvhNodesSrv.Reset();
	}

	m_worldSpaceTexelPositionTexture = createTexture2D(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_worldSpaceTexelPositionSRV = createShaderResourceView(m_worldSpaceTexelPositionTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelPositionUAV = createUnorderedAccessView(m_worldSpaceTexelPositionTexture.Get(), UAVPreset::Texture2D, 0);

	m_worldSpaceTexelNormalTexture = createTexture2D(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_worldSpaceTexelNormalSRV = createShaderResourceView(m_worldSpaceTexelNormalTexture.Get(), SRVPreset::Texture2D);
	m_worldSpaceTexelNormalUAV = createUnorderedAccessView(m_worldSpaceTexelNormalTexture.Get(), UAVPreset::Texture2D, 0);

	m_rtvCollector->addRTV(name + "::WorldSpaceTexelPosition", m_worldSpaceTexelPositionSRV.Get());
	m_rtvCollector->addRTV(name + "::WorldSpaceTexelNormal", m_worldSpaceTexelNormalSRV.Get());

	if (!prim)
		return;

	m_structuredVertexBuffer = createStructuredBuffer(sizeof(Vertex), static_cast<UINT>(prim->getVertexData().size()), SBPreset::CpuWrite);
	m_structuredVertexSRV = createShaderResourceView(m_structuredVertexBuffer.Get(), SRVPreset::StructuredBuffer);

	m_structuredIndexBuffer = createStructuredBuffer(sizeof(uint32_t), static_cast<UINT>(prim->getIndexData().size()), SBPreset::CpuWrite);
	m_structuredIndexSRV = createShaderResourceView(m_structuredIndexBuffer.Get(), SRVPreset::StructuredBuffer);

	m_bvhNodesBuffer = createStructuredBuffer(sizeof(BVH::Node), static_cast<UINT>(prim->getBVHNodes().size()), SBPreset::CpuWrite);
	m_bvhNodesSrv = createShaderResourceView(m_bvhNodesBuffer.Get(), SRVPreset::StructuredBuffer);

}

