#include <cstdint>
#include <d3d11.h>
#include <d3dcommon.h>

#include <dxgiformat.h>
#include <glm/gtc/matrix_transform.hpp>

#include "basePass.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/matrix.hpp"
#include "material.hpp"
#include "previewGenerator.hpp"
#include "primitiveData.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

using namespace Microsoft::WRL;

struct SphereVertex
{
	Position pos;
	TexCoords texCoords;
	Normal normal;
};

struct alignas(16) PreviewGenCB
{
	glm::mat4 modelViewProjection;
	glm::mat4 inverseTransposedModel;
	glm::mat4 model;
	glm::vec4 albedoColor;
	float metallicValue;
	float roughnessValue;
	uint32_t hasAlbedoTexture;
	uint32_t hasMetallicRoughnessTexture;
	uint32_t hasNormalTexture;
	float padding[3];
};

static constexpr int SPHERE_SEGMENTS = 32;
static constexpr int SPHERE_RINGS = 32;
static constexpr uint32_t SPHERE_VERTEX_COUNT = (SPHERE_RINGS + 1) * (SPHERE_SEGMENTS + 1);
static constexpr uint32_t SPHERE_INDEX_COUNT = SPHERE_RINGS * SPHERE_SEGMENTS * 6;

static SphereVertex GeneratedSphereVertices[SPHERE_VERTEX_COUNT];
static uint32_t GeneratedSphereIndices[SPHERE_INDEX_COUNT];


static constexpr D3D11_INPUT_ELEMENT_DESC FSQuadInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };

constexpr uint32_t PREVIEW_SIZE = 512;

PreviewGenerator::PreviewGenerator(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadPixelShader("previewGenerator", L"../../src/shaders/previewGenerator.hlsl", "PS");
	m_shaderManager->LoadVertexShader("previewGenerator", L"../../src/shaders/previewGenerator.hlsl", "VS");
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::WriteDepth);
	m_samplerState = createSamplerState(SamplerPreset::LinearWrap);

	m_depthTexture =
		createTexture2D(PREVIEW_SIZE, PREVIEW_SIZE, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
	m_depthStencilView = createDepthStencilView(m_depthTexture.Get(), DSVPreset::Texture2D);


	InitializeSphere();
	m_vertexBuffer = createVertexBuffer(sizeof(GeneratedSphereVertices), GeneratedSphereVertices);
	m_indexBuffer = createIndexBuffer(sizeof(GeneratedSphereIndices), GeneratedSphereIndices);

	m_constantBuffer = createConstantBuffer(sizeof(PreviewGenCB));

	m_device->CreateInputLayout(FSQuadInputLayoutDesc, ARRAYSIZE(FSQuadInputLayoutDesc),
		m_shaderManager->getVertexShaderBlob("previewGenerator")->GetBufferPointer(),
		m_shaderManager->getVertexShaderBlob("previewGenerator")->GetBufferSize(),
		&m_inputLayout);
}

void PreviewGenerator::createMatPreviewResources(Material* material)
{
	material->preview.t_preview = createTexture2D(PREVIEW_SIZE, PREVIEW_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM);
	material->preview.srv_preview = createShaderResourceView(material->preview.t_preview.Get(), SRVPreset::Texture2D);
	material->preview.rtv_preview = createRenderTargetView(material->preview.t_preview.Get(), RTVPreset::Texture2D);
	m_rtvCollector->addRTV("Mat: " + material->name + " preview", material->preview.srv_preview.Get());
}

void PreviewGenerator::generatePreview(Scene* scene)
{
	const auto matnames = scene->getMaterialNames();
	if (matnames.empty())
		return;

	for (const auto& name : matnames)
	{
		const auto mat = scene->getMaterial(name);
		if (!mat->needsPreviewUpdate)
			continue;
		if (mat->preview.t_preview == nullptr)
		{
			createMatPreviewResources(mat.get());
		}
		update(mat.get());

		setViewport(PREVIEW_SIZE, PREVIEW_SIZE);
		m_context->RSSetState(m_rasterizerState.Get());
		m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

		const uint32_t stride = sizeof(SphereVertex);
		const uint32_t offset = 0;
		const float clearColor[4] = { 0.2f, 0.2f, 0.2f, 0.2f };
		m_context->ClearRenderTargetView(mat->preview.rtv_preview.Get(), clearColor);
		m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		m_context->IASetInputLayout(m_inputLayout.Get());
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		m_context->PSSetShaderResources(0, 3, mat->getSRVs());
		m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

		m_context->VSSetShader(m_shaderManager->getVertexShader("previewGenerator"), nullptr, 0);
		m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
		m_context->PSSetShader(m_shaderManager->getPixelShader("previewGenerator"), nullptr, 0);
		m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
		m_context->OMSetRenderTargets(1, mat->preview.rtv_preview.GetAddressOf(), m_depthStencilView.Get());

		m_context->DrawIndexed(SPHERE_INDEX_COUNT, 0, 0);
		unbindRenderTargets(1);
		unbindShaderResources(0, 3);

		mat->needsPreviewUpdate = false;
	}
}

void PreviewGenerator::update(Material* material)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		// Generate front view matrix (camera looking at origin from front)
		glm::vec3 eyePos = glm::vec3(0.0f, 0.0f, 3.0f);
		glm::vec3 targetPos = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 upDir = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 viewMatrix = glm::lookAt(eyePos, targetPos, upDir);

		// Generate projection matrix
		float aspectRatio = static_cast<float>(PREVIEW_SIZE) / static_cast<float>(PREVIEW_SIZE);
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 1.0f, 100.0f);

		glm::mat4 model = glm::mat4(1.0f);
		glm::mat4 modelViewProjection = glm::transpose(projectionMatrix * viewMatrix * model);
		glm::mat4 inverseTransposedModel = glm::inverse(model);
		auto* data = static_cast<PreviewGenCB*>(mapped.pData);
		data->modelViewProjection = modelViewProjection;
		data->inverseTransposedModel = inverseTransposedModel;
		data->model = model;
		data->albedoColor = material->albedoColor;
		data->metallicValue = material->metallicValue;
		data->roughnessValue = material->roughnessValue;
		data->hasAlbedoTexture = material->albedo != nullptr ? 1 : 0;
		data->hasMetallicRoughnessTexture = material->metallicRoughness != nullptr ? 1 : 0;
		data->hasNormalTexture = material->normal != nullptr ? 1 : 0;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}

void PreviewGenerator::InitializeSphere()
{
	const float PI = 3.14159265358979323846f;

	// Generate vertices
	for (int ring = 0; ring <= SPHERE_RINGS; ++ring)
	{
		float phi = PI * static_cast<float>(ring) / static_cast<float>(SPHERE_RINGS);
		float y = cosf(phi);
		float sinPhi = sinf(phi);

		for (int seg = 0; seg <= SPHERE_SEGMENTS; ++seg)
		{
			float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(SPHERE_SEGMENTS);
			float x = sinPhi * cosf(theta);
			float z = sinPhi * sinf(theta);

			int index = ring * (SPHERE_SEGMENTS + 1) + seg;
			GeneratedSphereVertices[index].pos = { x, y, z };
			GeneratedSphereVertices[index].texCoords = { static_cast<float>(seg) / static_cast<float>(SPHERE_SEGMENTS),
														static_cast<float>(ring) / static_cast<float>(SPHERE_RINGS) };
			// For a unit sphere centered at origin, the normal is the same as the position
			GeneratedSphereVertices[index].normal = { x, y, z };
		}
	}

	// Generate indices
	int idx = 0;
	for (int ring = 0; ring < SPHERE_RINGS; ++ring)
	{
		for (int seg = 0; seg < SPHERE_SEGMENTS; ++seg)
		{
			int current = ring * (SPHERE_SEGMENTS + 1) + seg;
			int next = current + SPHERE_SEGMENTS + 1;

			GeneratedSphereIndices[idx++] = current;
			GeneratedSphereIndices[idx++] = next;
			GeneratedSphereIndices[idx++] = current + 1;

			GeneratedSphereIndices[idx++] = current + 1;
			GeneratedSphereIndices[idx++] = next;
			GeneratedSphereIndices[idx++] = next + 1;
		}
	}
}
