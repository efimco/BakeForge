
#include <d3d11_1.h>
#include <wrl.h>
#include <glm/glm.hpp>
#include <memory>

using namespace Microsoft::WRL;
class Scene;
class ShaderManager;
struct Texture;

class WorldSpaceUIPass
{
public:
	explicit WorldSpaceUIPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~WorldSpaceUIPass() = default;

	void draw(const glm::mat4& view,
		const glm::mat4& projection,
		Scene* scene,
		ComPtr<ID3D11RenderTargetView> objectIDRTV);
	void createOrResize();
	ComPtr<ID3D11RenderTargetView> getRTV();
	ComPtr<ID3D11ShaderResourceView> getSRV();

private:
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11ShaderResourceView> m_srv;

	std::shared_ptr<Texture> m_lightIconTexture;

	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
	void createQuad();

	ComPtr<ID3D11Buffer> m_lightsBuffer;
	void createLightsBuffer();

	ComPtr<ID3D11ShaderResourceView> m_lightsSRV;
	void updateLights(Scene* scene);

	ComPtr<ID3D11InputLayout> m_inputLayout;
	void createInputLayout();

	ComPtr<ID3D11SamplerState> m_samplerState;
	void createSamplerState();

	ComPtr<ID3D11Buffer> m_constantBuffer;
	void createConstantBuffer();
	void updateConstantBuffer(const glm::mat4& view, const glm::mat4& projection, Scene* scene);

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	void createRasterizerState();

	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	void createDepthStencilState();

	std::unique_ptr<ShaderManager> m_shaderManager;

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
};