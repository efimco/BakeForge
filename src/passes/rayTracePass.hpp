#pragma once

#include <d3d11.h>
#include "basePass.hpp"
#include "primitive.hpp"
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
	void update(Scene* scene, const Primitive* const&);
	ComPtr<ID3D11Texture2D> m_texture;

	ComPtr<ID3D11Buffer> m_bvhNodesBuffer;
	ComPtr<ID3D11ShaderResourceView> m_bvhNodesSrv;

	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
	Scene* m_scene;

	std::unique_ptr<RTVCollector> m_rtvCollector;
	ComPtr<ID3D11Buffer> m_constantBuffer;
};
