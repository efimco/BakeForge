#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "basePass.hpp"
#include "material.hpp"

using namespace Microsoft::WRL;


class Scene;
class RTVCollector;
struct Material;


class PreviewGenerator : public BasePass
{
public:
	explicit PreviewGenerator(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~PreviewGenerator() = default;

	void generatePreview(Scene* scene);

private:
	static void InitializeSphere();
	void createMatPreviewResources(Material* material);
	void update(Material* material);

	ComPtr<ID3D11Texture2D> m_depthTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	ComPtr<ID3D11Buffer> m_constantBuffer;

	std::unique_ptr<RTVCollector> m_rtvCollector;
};
