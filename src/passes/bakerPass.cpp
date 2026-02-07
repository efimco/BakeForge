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
#include "material.hpp"
#include "texture.hpp"


#define PROFILE_BAKER_PASS 1

struct alignas(16) BakerCB
{
	glm::mat4 worldMatrix;
	glm::mat4 worldMatrixInvTranspose;
	glm::uvec2 dimensions;
	float cageOffset;
	uint32_t useSmoothedNormals;
	uint32_t numBLASInstances;
	float padding[3];
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
	{"NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}, // smooth normal for baking
};


BakerPass::BakerPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, Scene* scene)
	: BasePass(device, context)
{
	m_device = device;
	m_context = context;
	m_scene = scene;

	m_rtvCollector = std::make_unique<RTVCollector>();

	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_shaderManager->LoadComputeShader("bakerBakeNormal", ShaderManager::GetShaderPath(L"baker.hlsl"), "CSBakeNormal");
	m_shaderManager->LoadVertexShader("raycastDebug", ShaderManager::GetShaderPath(L"raycastDebug.hlsl"), "VS");
	m_shaderManager->LoadPixelShader("raycastDebug", ShaderManager::GetShaderPath(L"raycastDebug.hlsl"), "PS");
	m_shaderManager->LoadVertexShader("uvRasterize", ShaderManager::GetShaderPath(L"uvRasterize.hlsl"), "VS");
	m_shaderManager->LoadPixelShader("uvRasterize", ShaderManager::GetShaderPath(L"uvRasterize.hlsl"), "PS");

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
	if (m_primitivesToBake.first.empty() || m_primitivesToBake.second.empty())
	{
		std::cerr << "BakerPass::bake: No primitives to bake!" << std::endl;
		return;
	}
	std::cout << "Started baking: " << name << std::endl;
	m_lastWidth = width;
	m_lastHeight = height;
	m_cageOffset = cageOffset;
	m_useSmoothedNormals = useSmoothedNormals;
	createInterpolatedTexturesResources();
	createBakedNormalResources();
	m_combinedHighPolyBuffers = createCombinedHighPolyBuffers();

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_context->ClearRenderTargetView(m_wsTexelPositionRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelNormalRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelTangentRTV.Get(), clearColor);
	m_context->ClearRenderTargetView(m_wsTexelSmoothedNormalRTV.Get(), clearColor);
	for (size_t i = 0; i < m_primitivesToBake.first.size(); ++i)
	{
		Primitive* lowPoly = m_primitivesToBake.first[i];
		if (!lowPoly)
			continue;
		rasterizeUVSpace(lowPoly);
	}
	updateBakerCB(m_combinedHighPolyBuffers);
	bakeNormals(m_combinedHighPolyBuffers);
	asyncSaveTextureToFile(directory + "\\" + filename,
		m_device,
		m_context,
		m_bakedNormalTexture);
}

void BakerPass::previewBakedNormal()
{
	std::string fullPath = directory + "\\" + filename;
	std::shared_ptr<Texture> newNormal = std::make_shared<Texture>(fullPath, m_device, m_context);
	auto material = m_primitivesToBake.first[0]->material;
	if (!material)
	{
		std::cerr << "No material assigned to the first low-poly primitive, cannot set baked normal preview." << std::endl;
		return;
	}
	m_scene->addTexture(newNormal);
	material->normal = newNormal;
	material->needsPreviewUpdate = true;
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

void BakerPass::setPrimitivesToBake(const std::pair<std::vector<Primitive*>, std::vector<Primitive*>>& primitivePairs)
{
	m_primitivesToBake = primitivePairs;
}

bool BakerPass::bakedNormalExists() const
{
	if (directory.empty() || filename.empty())
		return false;
	std::string fullPath = directory + "\\" + filename;
	return std::filesystem::exists(fullPath);
}

CombinedHighPolyBuffers BakerPass::createCombinedHighPolyBuffers()
{
	CombinedHighPolyBuffers combinedBuffers;

	size_t totalTriangles = 0;
	size_t totalTriIndices = 0;
	size_t totalBVHNodes = 0;

	for (Primitive* hp : m_primitivesToBake.second)
	{
		if (!hp) continue;
		totalTriangles += hp->getTriangles().size();
		totalTriIndices += hp->getTriangleIndices().size();
		totalBVHNodes += hp->getBVHNodes().size();
	}

	std::cout << "Building combined high-poly buffers:" << std::endl;
	std::cout << "  Total triangles: " << totalTriangles << std::endl;
	std::cout << "  Total tri indices: " << totalTriIndices << std::endl;
	std::cout << "  Total BVH nodes: " << totalBVHNodes << std::endl;
	std::cout << "  Number of BLAS instances: " << m_primitivesToBake.second.size() << std::endl;

	// Allocate combined CPU-side vectors
	std::vector<Triangle> allTriangles;
	std::vector<uint32_t> allTriIndices;
	std::vector<BVH::Node> allBVHNodes;
	std::vector<BLASInstance> blasInstances;

	allTriangles.reserve(totalTriangles);
	allTriIndices.reserve(totalTriIndices);
	allBVHNodes.reserve(totalBVHNodes);
	blasInstances.reserve(m_primitivesToBake.second.size());

	uint32_t triangleOffset = 0;
	uint32_t triIndicesOffset = 0;
	uint32_t bvhNodeOffset = 0;

	for (Primitive* hp : m_primitivesToBake.second)
	{
		if (!hp) continue;

		// Use references - NO COPY! Triangles/BVH stay in local space
		const auto& tris = hp->getTriangles();         // reference, not copy
		const auto& indices = hp->getTriangleIndices();
		const auto& nodes = hp->getBVHNodes();         // reference, not copy

		// Create BLAS instance with transforms for GPU-side ray transformation
		BLASInstance inst;
		inst.worldBBox = hp->getWorldBBox();  // Only this needs world-space (cheap)
		glm::mat4 worldMatrix = hp->getWorldMatrix();
		inst.worldMatrixInv = glm::transpose(glm::inverse(worldMatrix));  // Row-major for HLSL
		inst.normalMatrix = glm::transpose(inst.worldMatrixInv);
		inst.triangleOffset = triangleOffset;
		inst.triIndicesOffset = triIndicesOffset;
		inst.bvhNodeOffset = bvhNodeOffset;
		inst.numTriangles = static_cast<uint32_t>(tris.size());
		blasInstances.push_back(inst);

		// Append local-space data directly (no transform!)
		allTriangles.insert(allTriangles.end(), tris.begin(), tris.end());
		allTriIndices.insert(allTriIndices.end(), indices.begin(), indices.end());
		allBVHNodes.insert(allBVHNodes.end(), nodes.begin(), nodes.end());

		triangleOffset += static_cast<uint32_t>(tris.size());
		triIndicesOffset += static_cast<uint32_t>(indices.size());
		bvhNodeOffset += static_cast<uint32_t>(nodes.size());
	}
	combinedBuffers.numBLASInstances = static_cast<uint32_t>(blasInstances.size());


	// Create GPU buffers
	if (!allTriangles.empty())
	{
		combinedBuffers.triangleBuffer = createStructuredBuffer(sizeof(Triangle),
			static_cast<UINT>(allTriangles.size()), SBPreset::Immutable, allTriangles.data());
		combinedBuffers.trianglesSRV = createShaderResourceView(combinedBuffers.triangleBuffer.Get(), SRVPreset::StructuredBuffer);
	}

	if (!allTriIndices.empty())
	{
		combinedBuffers.triIndicesBuffer = createStructuredBuffer(sizeof(uint32_t),
			static_cast<UINT>(allTriIndices.size()), SBPreset::Immutable, allTriIndices.data());
		combinedBuffers.triIndicesSRV = createShaderResourceView(combinedBuffers.triIndicesBuffer.Get(), SRVPreset::StructuredBuffer);
	}

	if (!allBVHNodes.empty())
	{
		combinedBuffers.bvhNodesBuffer = createStructuredBuffer(sizeof(BVH::Node),
			static_cast<UINT>(allBVHNodes.size()), SBPreset::Immutable, allBVHNodes.data());
		combinedBuffers.bvhNodesSRV = createShaderResourceView(combinedBuffers.bvhNodesBuffer.Get(), SRVPreset::StructuredBuffer);
	}

	if (!blasInstances.empty())
	{
		combinedBuffers.blasInstancesBuffer = createStructuredBuffer(sizeof(BLASInstance),
			static_cast<UINT>(blasInstances.size()), SBPreset::Immutable, blasInstances.data());
		combinedBuffers.blasInstancesSRV = createShaderResourceView(combinedBuffers.blasInstancesBuffer.Get(), SRVPreset::StructuredBuffer);
	}


	return combinedBuffers;
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


void BakerPass::bakeNormals(const CombinedHighPolyBuffers& combinedBuffers)
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

	ID3D11ShaderResourceView* hpSRVs[8] = {
		combinedBuffers.blasInstancesSRV.Get(),
		combinedBuffers.trianglesSRV.Get(),
		combinedBuffers.triIndicesSRV.Get(),
		combinedBuffers.bvhNodesSRV.Get(),
		m_wsTexelPositionSRV.Get(),
		m_wsTexelNormalSRV.Get(),
		m_wsTexelTangentSRV.Get(),
		m_wsTexelSmoothedNormalSRV.Get()
	};

	m_context->CSSetShaderResources(0, 8, hpSRVs);
	UINT threadGroupX = (m_lastWidth + 15) / 16;
	UINT threadGroupY = (m_lastHeight + 15) / 16;
	m_context->Dispatch(threadGroupX, threadGroupY, 1);
	unbindComputeUAVs(0, 1);
	unbindShaderResources(0, 8);

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

void BakerPass::updateBakerCB(const CombinedHighPolyBuffers& combinedBuffers)
{
	// Update constant buffer with numBLASInstances
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		auto* data = static_cast<BakerCB*>(mapped.pData);
		data->dimensions = glm::uvec2(m_lastWidth, m_lastHeight);
		data->worldMatrix = glm::mat4(1.0f); // Identity - triangles already in world space
		data->worldMatrixInvTranspose = glm::mat4(1.0f);
		data->cageOffset = m_cageOffset;
		data->useSmoothedNormals = m_useSmoothedNormals;
		data->numBLASInstances = combinedBuffers.numBLASInstances;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
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

	m_saveTextureFuture = std::async(std::launch::async, [fullPath, image = std::move(capturedImage)]() mutable
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

	m_bakedNormalTexture = createTexture2D(m_lastWidth, m_lastHeight, DXGI_FORMAT_R16G16B16A16_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_bakedNormalSRV = createShaderResourceView(m_bakedNormalTexture.Get(), SRVPreset::Texture2D);
	m_bakedNormalUAV = createUnorderedAccessView(m_bakedNormalTexture.Get(), UAVPreset::Texture2D, 0);

	m_rtvCollector->addRTV(name + "::BakedNormalTexture", m_bakedNormalSRV.Get());
}

