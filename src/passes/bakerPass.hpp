#pragma once

#include "basePass.hpp"
#include "glm/glm.hpp"
#include <string>

class Scene;
class RTVCollector;
class Primitive;

struct LowPolyPrimitiveBuffers
{
	ComPtr<ID3D11Buffer> structuredVertexBuffer;
	ComPtr<ID3D11Buffer> structuredIndexBuffer;
	ComPtr<ID3D11ShaderResourceView> structuredVertexSRV;
	ComPtr<ID3D11ShaderResourceView> structuredIndexSRV;
};

struct HighPolyPrimitiveBuffers
{
	ComPtr<ID3D11ShaderResourceView> srv_triangleBuffer;
	ComPtr<ID3D11ShaderResourceView> srv_triangleIndicesBuffer;
	ComPtr<ID3D11Buffer> bvhNodesBuffer;
	ComPtr<ID3D11ShaderResourceView> bvhNodesSRV;
};

class BakerPass : public BasePass
{
public:
	explicit BakerPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~BakerPass() override = default;

	std::string name = "Baker Pass";

	void bake(uint32_t width, uint32_t height, float cageOffset, uint32_t useSmoothedNormals);
	void drawRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);
	void createOrResize();
	void setPrimitivesToBake(const std::vector<std::pair<Primitive*, Primitive*>>& primitivePairs);
	std::vector<std::pair<Primitive*, Primitive*>> getPrimitivesToBake() const { return m_primitivePairs; }
	std::string directory = "";
	std::string filename = "";

private:
	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	std::vector<std::pair<Primitive*, Primitive*>> m_primitivePairs;

	HighPolyPrimitiveBuffers createHighPolyPrimitiveBuffers(Primitive* prim);
	void updateHighPolyPrimitiveBuffers(Primitive* prim, const HighPolyPrimitiveBuffers& buffers);

	void bakeNormals(const HighPolyPrimitiveBuffers& hpBuffers);
	void saveToTextureFile();

	ComPtr<ID3D11Buffer> m_constantBuffer;

	float m_cageOffset = 0.1f;
	uint32_t m_useSmoothedNormals = 0;

	ComPtr<ID3D11Texture2D> m_wsTexelPositionTexture;
	ComPtr<ID3D11ShaderResourceView> m_wsTexelPositionSRV;
	ComPtr<ID3D11UnorderedAccessView> m_wsTexelPositionUAV;

	ComPtr<ID3D11Texture2D> m_wsTexelNormalTexture;
	ComPtr<ID3D11ShaderResourceView> m_wsTexelNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_wsTexelNormalUAV;

	ComPtr<ID3D11Texture2D> m_wsTexelTangentTexture;
	ComPtr<ID3D11ShaderResourceView> m_wsTexelTangentSRV;
	ComPtr<ID3D11UnorderedAccessView> m_wsTexelTangentUAV;

	ComPtr<ID3D11Texture2D> m_wsTexelSmoothedNormalTexture;
	ComPtr<ID3D11ShaderResourceView> m_wsTexelSmoothedNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_wsTexelSmoothedNormalUAV;

	ComPtr<ID3D11Texture2D> m_bakedNormalTexture;
	ComPtr<ID3D11ShaderResourceView> m_bakedNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_bakedNormalUAV;

	ComPtr<ID3D11Texture2D> m_raycastVisualizationTexture;
	ComPtr<ID3D11ShaderResourceView> m_raycastVisualizationSRV;
	ComPtr<ID3D11RenderTargetView> m_raycastVisualizationRTV;

	ComPtr<ID3D11Texture2D> m_raycastDepthTexture;
	ComPtr<ID3D11DepthStencilView> m_raycastDepthDSV;
	ComPtr<ID3D11DepthStencilState> m_raycastDepthStencilState;
	ComPtr<ID3D11RasterizerState> m_raycastRasterizerState;
	ComPtr<ID3D11Buffer> m_raycastConstantBuffer;

	ComPtr<ID3D11Query> m_disjointQuery;
	ComPtr<ID3D11Query> m_startQuery;
	ComPtr<ID3D11Query> m_endQuery;

	// UV Rasterization resources
	ComPtr<ID3D11RenderTargetView> m_wsTexelPositionRTV;
	ComPtr<ID3D11RenderTargetView> m_wsTexelNormalRTV;
	ComPtr<ID3D11RenderTargetView> m_wsTexelTangentRTV;
	ComPtr<ID3D11RenderTargetView> m_wsTexelSmoothedNormalRTV;
	ComPtr<ID3D11InputLayout> m_uvRasterInputLayout;
	ComPtr<ID3D11RasterizerState> m_uvRasterRasterizerState;
	ComPtr<ID3D11DepthStencilState> m_uvRasterDepthStencilState;

	void rasterizeUVSpace(Primitive* lowPoly);
	void updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);

	void createInterpolatedTexturesResources();
	void createBakedNormalResources();

	std::unique_ptr<RTVCollector> m_rtvCollector;
};