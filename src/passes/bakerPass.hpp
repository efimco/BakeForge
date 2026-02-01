#pragma once

#include "basePass.hpp"
#include <string>
#include <future>

#include "glm/glm.hpp"
#include "bvhNode.hpp"


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

struct CombinedHighPolyBuffers
{
	ComPtr<ID3D11Buffer> triangleBuffer;
	ComPtr<ID3D11Buffer> triIndicesBuffer;
	ComPtr<ID3D11Buffer> bvhNodesBuffer;
	ComPtr<ID3D11Buffer> blasInstancesBuffer;

	ComPtr<ID3D11ShaderResourceView> trianglesSRV;
	ComPtr<ID3D11ShaderResourceView> triIndicesSRV;
	ComPtr<ID3D11ShaderResourceView> bvhNodesSRV;
	ComPtr<ID3D11ShaderResourceView> blasInstancesSRV;

	uint32_t numBLASInstances = 0;
};

// Instance data for TLAS - references into combined buffers
// Triangles/BVH nodes stay in local space, ray is transformed to local space on GPU
struct BLASInstance
{
	BVH::BBox worldBBox;         // 32 bytes - for early TLAS culling
	glm::mat4 worldMatrixInv;    // 64 bytes - transforms ray from world to local space
	glm::mat4 normalMatrix;      // 64 bytes - transforms normals from local to world (transpose of inverse)
	uint32_t triangleOffset;     // offset into combined triangle buffer
	uint32_t triIndicesOffset;   // offset into combined indices buffer
	uint32_t bvhNodeOffset;      // offset into combined BVH nodes buffer
	uint32_t numTriangles;       // number of triangles in this BLAS
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
	void setPrimitivesToBake(const std::pair<std::vector<Primitive*>, std::vector<Primitive*>>& primitivePairs);
	std::pair<std::vector<Primitive*>, std::vector<Primitive*>> getPrimitivesToBake() const { return m_primitivesToBake; }
	std::string directory = "";
	std::string filename = "";

private:
	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	std::pair<std::vector<Primitive*>, std::vector<Primitive*>> m_primitivesToBake;


	std::future<void> m_saveTextureFuture;

	ComPtr<ID3D11Buffer> m_constantBuffer;

	float m_cageOffset = 0.1f;
	uint32_t m_useSmoothedNormals = 0;

	CombinedHighPolyBuffers m_combinedHighPolyBuffers;

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

	CombinedHighPolyBuffers createCombinedHighPolyBuffers();

	void bakeNormals(const CombinedHighPolyBuffers& hpBuffers);

	void updateBakerCB(const CombinedHighPolyBuffers& combinedBuffers);

	void saveToTextureFile();
	void asyncSaveTextureToFile(const std::string& fullPath,
		ComPtr<ID3D11Device> device,
		ComPtr<ID3D11DeviceContext> context,
		ComPtr<ID3D11Texture2D> texture);

	void rasterizeUVSpace(Primitive* lowPoly);
	void updateRaycastVisualization(const glm::mat4& view, const glm::mat4& projection);

	void createInterpolatedTexturesResources();
	void createBakedNormalResources();

	std::unique_ptr<RTVCollector> m_rtvCollector;
};