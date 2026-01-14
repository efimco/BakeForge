#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <memory>
#include <wrl.h>

#include "basePass.hpp"


using namespace Microsoft::WRL;
class Scene;
class ShaderManager;
struct Texture;
class RTVCollector;

class WorldSpaceUIPass : public BasePass
{
public:
	explicit WorldSpaceUIPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~WorldSpaceUIPass() override = default;

	void
	draw(const glm::mat4& view, const glm::mat4& projection, Scene* scene, ComPtr<ID3D11RenderTargetView> objectIDRTV);
	void createOrResize();
	ComPtr<ID3D11RenderTargetView> getRTV();
	ComPtr<ID3D11ShaderResourceView> getSRV();

private:
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11ShaderResourceView> m_srv;

	std::unique_ptr<RTVCollector> m_rtvCollector;

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


	ComPtr<ID3D11Buffer> m_constantBuffer;
	void createConstantBuffer();
	void updateConstantBuffer(const glm::mat4& view, const glm::mat4& projection, Scene* scene);
};
