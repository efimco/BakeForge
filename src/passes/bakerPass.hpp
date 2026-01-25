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

	void bake(uint32_t width, uint32_t heightm, float cageOffset);
	void drawRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);
	void createOrResize();
	void setPrimitivesToBake(const std::vector<std::pair<Primitive*, Primitive*>>& primitivePairs);

private:
	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	std::vector<std::pair<Primitive*, Primitive*>> m_primitivePairs;

	LowPolyPrimitiveBuffers createLowPolyPrimitiveBuffers(Primitive* prim);
	HighPolyPrimitiveBuffers createHighPolyPrimitiveBuffers(Primitive* prim);
	void updateLowPolyPrimitiveBuffers(Primitive* prim, const LowPolyPrimitiveBuffers& buffers);
	void updateHighPolyPrimitiveBuffers(Primitive* prim, const HighPolyPrimitiveBuffers& buffers);

	void createInterpolatedTextures();
	void bakeNormals(const HighPolyPrimitiveBuffers& hpBuffers);

	ComPtr<ID3D11Buffer> m_constantBuffer;

	float m_cageOffset = 0.1f;

	ComPtr<ID3D11Texture2D> m_worldSpaceTexelPositionTexture;
	ComPtr<ID3D11ShaderResourceView> m_worldSpaceTexelPositionSRV;
	ComPtr<ID3D11UnorderedAccessView> m_worldSpaceTexelPositionUAV;

	ComPtr<ID3D11Texture2D> m_worldSpaceTexelNormalTexture;
	ComPtr<ID3D11ShaderResourceView> m_worldSpaceTexelNormalSRV;
	ComPtr<ID3D11UnorderedAccessView> m_worldSpaceTexelNormalUAV;

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

	void updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);

	void createInterpolatedTexturesResources();
	void createBakedNormalResources();

	std::unique_ptr<RTVCollector> m_rtvCollector;
};