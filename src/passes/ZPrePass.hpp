#pragma once

#include <memory>

#include <d3d11_1.h>
#include <wrl.h>
#include <glm/glm.hpp>

class Camera;
class ShaderManager;
class Primitive;
class Scene;

using namespace Microsoft::WRL;

class ZPrePass
{
public:
	ZPrePass( ComPtr<ID3D11Device> device,  ComPtr<ID3D11DeviceContext> context);
	~ZPrePass() = default;

	void draw(const glm::mat4& view, const glm::mat4& projection, Scene* scene);
	ComPtr<ID3D11DepthStencilView> getDSV();
	void createOrResize();

	ComPtr<ID3D11ShaderResourceView> getDepthSRV() const;
private:
	void update(const glm::mat4& view, const glm::mat4& projection, Primitive* prim);

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11Texture2D> t_depth;

	ComPtr<ID3D11DepthStencilView> dsv;

	ComPtr<ID3D11ShaderResourceView> srv_depth;

	ComPtr<ID3D11Buffer> m_constantbuffer;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11InputLayout> m_inputLayout;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	std::unique_ptr<ShaderManager> m_shaderManager;
};
