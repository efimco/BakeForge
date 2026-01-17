#pragma once

#include "basePass.hpp"
#include "rtvCollector.hpp"

class Scene;
class RTVCollector;


class RayTracePass : BasePass
{
public:
	RayTracePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~RayTracePass() = default;
	void draw(Scene* scene);
	void createOrResize();

private:
	void update(Scene* scene);
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
	Scene* m_scene;

	std::unique_ptr<RTVCollector> m_rtvCollector;
	ComPtr<ID3D11Buffer> m_constantBuffer;
};
