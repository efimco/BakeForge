#include <d3d11.h>

#include "basePass.hpp"
#include "previewGenerator.hpp"
#include "primitiveData.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

using namespace Microsoft::WRL;

struct Vertex
{
	Position pos;
	TexCoords texCoords;
};

static constexpr int SPHERE_SEGMENTS = 32;
static constexpr int SPHERE_RINGS = 32;
static constexpr uint32_t SPHERE_VERTEX_COUNT = (SPHERE_RINGS + 1) * (SPHERE_SEGMENTS + 1);
static constexpr uint32_t SPHERE_INDEX_COUNT = SPHERE_RINGS * SPHERE_SEGMENTS * 6;

static Vertex GeneratedSphereVertices[SPHERE_VERTEX_COUNT];
static uint32_t GeneratedSphereIndices[SPHERE_INDEX_COUNT];

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
			GeneratedSphereVertices[index].pos = {x, y, z};
			GeneratedSphereVertices[index].texCoords = {static_cast<float>(seg) / static_cast<float>(SPHERE_SEGMENTS),
														static_cast<float>(ring) / static_cast<float>(SPHERE_RINGS)};
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


static constexpr D3D11_INPUT_ELEMENT_DESC FSQuadInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


PreviewGenerator::PreviewGenerator(const int width,
								   const int height,
								   ComPtr<ID3D11Device> device,
								   ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	InitializeSphere();
	m_width = width;
	m_height = height;
	m_shaderManager->LoadComputeShader("previewGenerator", L"../../src/shaders/previewGenerator.hlsl", "CS");
	m_rasterizerState = createRSState(RasterizerPreset::Default);
	m_depthStencilState = createDSState(DepthStencilPreset::ReadOnlyEqual);
	m_samplerState = createSamplerState(SamplerPreset::LinearWrap);
	m_texture = createTexture2D(m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_renderTargetView = createRenderTargetView(m_texture.Get(), RTVPreset::Texture2D);
}


PreviewGenerator::~PreviewGenerator()
{
}

void PreviewGenerator::generatePreview(Scene* scene)
{
}
