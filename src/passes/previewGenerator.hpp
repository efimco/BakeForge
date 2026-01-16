#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "basePass.hpp"

using namespace Microsoft::WRL;


class Scene;


class PreviewGenerator : public BasePass
{
public:
	explicit PreviewGenerator(const int width,
							  const int height,
							  ComPtr<ID3D11Device> device,
							  ComPtr<ID3D11DeviceContext> context);
	~PreviewGenerator();

	void generatePreview(Scene* scene);

private:
	int m_width;
	int m_height;
	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

	static void InitializeSphere();

	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
};
