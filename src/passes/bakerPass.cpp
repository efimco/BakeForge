#include "bakerPass.hpp"

#include <iostream>

#include "glm/glm.hpp"
#include "DirectXTex.h"

#include "appConfig.hpp"
#include "primitive.hpp"
#include "primitiveData.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"


#define PROFILE_BAKER_PASS 1

struct alignas(16) BakerCB
{
	glm::mat4 worldMatrix;
	glm::mat4 worldMatrixInvTranspose;
	glm::uvec2 dimensions;
	float cageOffset;
	uint32_t useSmoothedNormals;
};

struct alignas(16) RaycastVisCB
{
	glm::mat4 viewProjection;
	glm::uvec2 textureDimensions;
	float rayLength;
	float padding;
};

// input layout for UV 	ization (matches Vertex struct)
static constexpr D3D11_INPUT_ELEMENT_DESC uvRasterInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


BakerPass::BakerPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_device = device;
	m_context = context;

	m_rtvCollector = std::make_unique<RTVCollector>();

	m_shaderManager = std::make_unique<ShaderManager>(device);
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

void BakerPass::bake(uint32_t width, uint32_t height, float cageOffset, uint32_t useSmoothedNormals)
{
	if (directory.empty() || filename.empty())
	{
		std::cerr << "BakerPass::bake: No output path specified!" << std::endl;
		return;
	}
	std::cout << "Started baking: " << name << std::endl;
	m_lastWidth = width;
	m_lastHeight = height;
	m_cageOffset = cageOffset;
	m_useSmoothedNormals = useSmoothedNormals;
	createInterpolatedTexturesResources();
	createBakedNormalResources();
	for (const auto& [lowPoly, highPoly] : m_primitivePairs)
	{
		if (!lowPoly || !highPoly)
			continue;

		auto hpBuffers = createHighPolyPrimitiveBuffers(highPoly);
		updateHighPolyPrimitiveBuffers(highPoly, hpBuffers);

		rasterizeUVSpace(lowPoly);

		bakeNormals(hpBuffers);
		asyncSaveTextureToFile(directory + "\\" + filename,
			m_device,
			m_context,
			m_bakedNormalTexture);
	}
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
	m_context->VSSetShaderResources(0, 1, m_wsTexelPositionSRV.GetAddressOf());
	m_context->VSSetShaderResources(1, 1, m_wsTexelNormalSRV.GetAddressOf());
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

#if PROFILE_BAKER_PASS
	m_context->Begin(m_disjointQuery.Get());
	m_context->End(m_startQuery.Get());
#endif

	beginDebugEvent(L"Baker::Rasterize UV Space");
	std::cout << "Rasterizing UV space for primitive: " << lowPoly->name << std::endl;
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<BakerCB*>(mapped.pData);
		data->dimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->worldMatrix = glm::transpose(lowPoly->getWorldMatrix());
		data->worldMatrixInvTranspose = glm::inverse(lowPoly->getWorldMatrix());
		data->cageOffset = m_cageOffset;
		data->useSmoothedNormals = m_useSmoothedNormals;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}


	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_context->ClearRenderTargetView(m_wsTexelPositionRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelNormalRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelTangentRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelSmoothedNormalRTV.Get(), clearColor);

	setViewport(m_lastWidth, m_lastHeight);

	ID3D11RenderTargetView* rtvs[4] = { m_wsTexelPositionRTV.Get(),
		 m_wsTexelNormalRTV.Get(),
		 m_wsTexelTangentRTV.Get(),
		 m_wsTexelSmoothedNormalRTV.Get() };
	m_context->OMSetRenderTargets(4, rtvs, nullptr);

	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetInputLayout(m_uvRasterInputLayout.Get());
	m_context->RSSetState(m_uvRasterRasterizerState.Get());
	m_context->OMSetDepthStencilState(m_uvRasterDepthStencilState.Get(), 0);

	m_context->VSSetShader(m_shaderManager->getVertexShader("uvRasterize"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("uvRasterize"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	auto vb = lowPoly->getVertexBuffer();
	auto ib = lowPoly->getIndexBuffer();
	m_context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
	m_context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

	m_context->DrawIndexed(static_cast<UINT>(lowPoly->getIndexData().size()), 0, 0);

	unbindRenderTargets(4);

	endDebugEvent();

#if PROFILE_BAKER_PASS
	m_context->End(m_endQuery.Get());
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

}


void BakerPass::bakeNormals(const HighPolyPrimitiveBuffers& hpBuffers)
{
	beginDebugEvent(L"Baker::Bake Normals");

#if PROFILE_BAKER_PASS
	m_context->Begin(m_disjointQuery.Get());
	m_context->End(m_startQuery.Get());
#endif

	m_context->CSSetShader(m_shaderManager->getComputeShader("bakerBakeNormal"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	ID3D11UnorderedAccessView* bakedNormalUAVs[1] = { m_bakedNormalUAV.Get() };
	m_context->CSSetUnorderedAccessViews(0, 1, bakedNormalUAVs, nullptr);
	ID3D11ShaderResourceView* hpSRVs[7] = {
		hpBuffers.srv_triangleBuffer.Get(),
		hpBuffers.srv_triangleIndicesBuffer.Get(),
		hpBuffers.bvhNodesSRV.Get(),
		m_wsTexelPositionSRV.Get(),
		m_wsTexelNormalSRV.Get(),
		m_wsTexelTangentSRV.Get(),
		m_wsTexelSmoothedNormalSRV.Get()
	};
	m_context->CSSetShaderResources(0, 7, hpSRVs);
	UINT threadGroupX = (m_lastWidth + 15) / 16;
	UINT threadGroupY = (m_lastHeight + 15) / 16;
	m_context->Dispatch(threadGroupX, threadGroupY, 1);
	unbindComputeUAVs(0, 1);
	unbindShaderResources(0, 7);

	endDebugEvent();

#if PROFILE_BAKER_PASS
	m_context->End(m_endQuery.Get());
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
		std::cout << "GPU baking took " << gpuTimeMs << " ms" << std::endl;
	}
#endif
}

void BakerPass::saveToTextureFile()
{
	std::string fullPath = directory + "\\" + filename;
	std::cout << "Saving baked normal texture to: " << fullPath << std::endl;
	DirectX::ScratchImage image;
	if (FAILED(DirectX::CaptureTexture(m_device.Get(), m_context.Get(),
		m_bakedNormalTexture.Get(), image)))
	{
		std::cerr << "Failed to capture texture for saving." << std::endl;
		return;
	}
	if (filename.ends_with(".png"))
	{
		HRESULT hr = DirectX::SaveToWICFile(
			*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE,
			DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
			std::wstring(fullPath.begin(), fullPath.end()).c_str());
	}
	else if (filename.ends_with(".tga"))
	{
		HRESULT hr = DirectX::SaveToTGAFile(
			*image.GetImage(0, 0, 0),
			DirectX::TGA_FLAGS_NONE,
			std::wstring(fullPath.begin(), fullPath.end()).c_str());
	}
	else
	{
		std::cerr << "Unsupported file format for saving texture: " << filename << std::endl;
	}
}

void BakerPass::asyncSaveTextureToFile(const std::string& fullPath,
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> context,
	ComPtr<ID3D11Texture2D> texture)

{
	std::cout << "Saving baked normal texture to: " << fullPath << std::endl;
	DirectX::ScratchImage capturedImage;
	if (FAILED(DirectX::CaptureTexture(device.Get(), context.Get(),
		texture.Get(), capturedImage)))
	{
		std::cerr << "Failed to capture texture for saving." << std::endl;
		return;
	}

	DirectX::ScratchImage convertedImage;
	HRESULT hr = DirectX::Convert(
		*capturedImage.GetImage(0, 0, 0),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DirectX::TEX_FILTER_DEFAULT,
		DirectX::TEX_THRESHOLD_DEFAULT,
		convertedImage);

	if (FAILED(hr))
	{
		std::cerr << "Failed to convert texture format." << std::endl;
		return;
	}


	m_saveTextureFuture = std::async(std::launch::async, [fullPath, image = std::move(convertedImage)]() mutable
		{

			if (fullPath.ends_with(".png"))
			{
				HRESULT hr = DirectX::SaveToWICFile(
					*image.GetImage(0, 0, 0),
					DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
					std::wstring(fullPath.begin(), fullPath.end()).c_str());
				std::cout << "Finished saving texture to: " << fullPath << std::endl;
			}
			else if (fullPath.ends_with(".tga"))
			{
				HRESULT hr = DirectX::SaveToTGAFile(
					*image.GetImage(0, 0, 0),
					DirectX::TGA_FLAGS_NONE,
					std::wstring(fullPath.begin(), fullPath.end()).c_str());
				std::cout << "Finished saving texture to: " << fullPath << std::endl;
			}
			else
			{
				std::cerr << "Unsupported file format for saving texture: " << fullPath << std::endl;
			}
		});
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
	if (m_wsTexelPositionTexture != nullptr)
	{
		m_wsTexelPositionTexture.Reset();
		m_wsTexelPositionSRV.Reset();
		m_wsTexelPositionUAV.Reset();
		m_wsTexelPositionRTV.Reset();

		m_wsTexelNormalTexture.Reset();
		m_wsTexelNormalSRV.Reset();
		m_wsTexelNormalUAV.Reset();
		m_wsTexelNormalRTV.Reset();

		m_wsTexelTangentTexture.Reset();
		m_wsTexelTangentSRV.Reset();
		m_wsTexelTangentUAV.Reset();
		m_wsTexelTangentRTV.Reset();

		m_wsTexelSmoothedNormalTexture.Reset();
		m_wsTexelSmoothedNormalSRV.Reset();
		m_wsTexelSmoothedNormalUAV.Reset();
		m_wsTexelSmoothedNormalRTV.Reset();
	}

	std::cout << "Creating interpolated textures of size: " << m_lastWidth << "x" << m_lastHeight << std::endl;
	m_wsTexelPositionTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_wsTexelPositionSRV = createShaderResourceView(m_wsTexelPositionTexture.Get(), SRVPreset::Texture2D);
	m_wsTexelPositionUAV = createUnorderedAccessView(m_wsTexelPositionTexture.Get(), UAVPreset::Texture2D, 0);
	m_wsTexelPositionRTV = createRenderTargetView(m_wsTexelPositionTexture.Get(), RTVPreset::Texture2D);

	m_wsTexelNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_wsTexelNormalSRV = createShaderResourceView(m_wsTexelNormalTexture.Get(), SRVPreset::Texture2D);
	m_wsTexelNormalUAV = createUnorderedAccessView(m_wsTexelNormalTexture.Get(), UAVPreset::Texture2D, 0);
	m_wsTexelNormalRTV = createRenderTargetView(m_wsTexelNormalTexture.Get(), RTVPreset::Texture2D);

	m_wsTexelTangentTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_wsTexelTangentSRV = createShaderResourceView(m_wsTexelTangentTexture.Get(), SRVPreset::Texture2D);
	m_wsTexelTangentUAV = createUnorderedAccessView(m_wsTexelTangentTexture.Get(), UAVPreset::Texture2D, 0);
	m_wsTexelTangentRTV = createRenderTargetView(m_wsTexelTangentTexture.Get(), RTVPreset::Texture2D);

	m_wsTexelSmoothedNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET);
	m_wsTexelSmoothedNormalSRV = createShaderResourceView(m_wsTexelSmoothedNormalTexture.Get(), SRVPreset::Texture2D);
	m_wsTexelSmoothedNormalUAV = createUnorderedAccessView(m_wsTexelSmoothedNormalTexture.Get(), UAVPreset::Texture2D, 0);
	m_wsTexelSmoothedNormalRTV = createRenderTargetView(m_wsTexelSmoothedNormalTexture.Get(), RTVPreset::Texture2D);

	m_rtvCollector->addRTV(name + "::WorldSpaceTexelPosition", m_wsTexelPositionSRV.Get());
	m_rtvCollector->addRTV(name + "::WorldSpaceTexelNormal", m_wsTexelNormalSRV.Get());
	m_rtvCollector->addRTV(name + "::WorldSpaceTexelTangent", m_wsTexelTangentSRV.Get());
	m_rtvCollector->addRTV(name + "::WorldSpaceTexelSmoothedNormal", m_wsTexelSmoothedNormalSRV.Get());
}

void BakerPass::createBakedNormalResources()
{
	if (m_bakedNormalTexture != nullptr)
	{
		m_bakedNormalTexture.Reset();
		m_bakedNormalSRV.Reset();
		m_bakedNormalUAV.Reset();
	}
	std::cout << "Creating baked normal texture of size: " << m_lastWidth << "x" << m_lastHeight << std::endl;

	m_bakedNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_bakedNormalSRV = createShaderResourceView(m_bakedNormalTexture.Get(), SRVPreset::Texture2D);
	m_bakedNormalUAV = createUnorderedAccessView(m_bakedNormalTexture.Get(), UAVPreset::Texture2D, 0);

	m_rtvCollector->addRTV(name + "::BakedNormalTexture", m_bakedNormalSRV.Get());
}

