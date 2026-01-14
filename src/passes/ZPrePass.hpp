#pragma once

#include <memory>

#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <wrl.h>

#include "basePass.hpp"

class Camera;
class ShaderManager;
class Primitive;
class Scene;
class RTVCollector;

using namespace Microsoft::WRL;

class ZPrePass : public BasePass
{
public:
	ZPrePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~ZPrePass() override = default;

	void draw(const glm::mat4& view, const glm::mat4& projection, Scene* scene);
	ComPtr<ID3D11DepthStencilView> getDSV();
	void createOrResize();

	ComPtr<ID3D11ShaderResourceView> getDepthSRV() const;

private:
	void update(const glm::mat4& view, const glm::mat4& projection, Primitive* prim) const;

	std::unique_ptr<RTVCollector> m_rtvCollector;

	ComPtr<ID3D11Texture2D> t_depth;

	ComPtr<ID3D11DepthStencilView> dsv;

	ComPtr<ID3D11ShaderResourceView> srv_depth;

	ComPtr<ID3D11Buffer> m_constantbuffer;
	ComPtr<ID3D11InputLayout> m_inputLayout;
};
