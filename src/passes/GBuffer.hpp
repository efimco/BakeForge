#include <d3d11.h>
#include <wrl.h>
#include "camera.hpp"
#include <glm/glm.hpp>
#include "shaderManager.hpp"


using namespace Microsoft::WRL;

class GBuffer
{
public:
	GBuffer(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~GBuffer();

	void draw(glm::mat4 view, glm::mat4 projection);
	void update(glm::mat4 view, glm::mat4 projection);

private:

	void createOrResize();

	ComPtr<ID3D11Texture2D> t_albedo;
	ComPtr<ID3D11Texture2D> t_metallicRoughness;
	ComPtr<ID3D11Texture2D> t_normal;
	ComPtr<ID3D11Texture2D> t_position;
	ComPtr<ID3D11Texture2D> t_depth;

	ComPtr<ID3D11RenderTargetView> rtv_albedo;
	ComPtr<ID3D11RenderTargetView> rtv_metallicRoughness;
	ComPtr<ID3D11DepthStencilView> dsv;
	ComPtr<ID3D11RenderTargetView> rtv_normal;
	ComPtr<ID3D11RenderTargetView> rtv_position;

	ComPtr<ID3D11ShaderResourceView> srv_albedo;
	ComPtr<ID3D11ShaderResourceView> srv_metallicRoughness;
	ComPtr<ID3D11ShaderResourceView> srv_depth;
	ComPtr<ID3D11ShaderResourceView> srv_normal;
	ComPtr<ID3D11ShaderResourceView> srv_position;

	ComPtr<ID3D11Buffer> m_constantbuffer;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11InputLayout> m_inputLayout;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	ID3D11RenderTargetView* m_rtvs[4];

	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;

	ShaderManager* m_shaderManager;
};
