#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

using namespace Microsoft::WRL;

class Scene;
class ShaderManager;
class Primitive;

struct DebugLineVertex
{
	glm::vec3 position;
	glm::vec4 color;
};

class DebugBVHPass
{
public:
	DebugBVHPass(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context);
	~DebugBVHPass() = default;

	void draw(const glm::mat4& view, const glm::mat4& projection, Scene* scene,
		const ComPtr<ID3D11RenderTargetView>& rtv,
		const ComPtr<ID3D11DepthStencilView>& dsv, int maxDepth = -1);

	void setEnabled(bool enabled) { m_enabled = enabled; }
	bool isEnabled() const { return m_enabled; }

	void setMaxDepth(int depth) { m_maxDepth = depth; }
	int getMaxDepth() const { return m_maxDepth; }
	
	void setShowPrimitiveBVH(bool show) { m_showPrimitiveBVH = show; }
	bool getShowPrimitiveBVH() const { return m_showPrimitiveBVH; }

private:
	void updateVertexBuffer(Scene* scene, int maxDepth);
	void addBoxLines(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);
	void addPrimitiveBVHBoxes(Primitive* primitive, int maxDepth);

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	std::unique_ptr<ShaderManager> m_shaderManager;
	std::vector<DebugLineVertex> m_vertices;
	size_t m_maxVertices = 500000; // Max line vertices

	bool m_enabled = false;
	bool m_showPrimitiveBVH = false;
	int m_maxDepth = -1; // -1 means show all levels
};
