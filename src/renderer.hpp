#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "dxDevice.hpp"
#include "shaderManager.hpp"
#include "uiManager.hpp"
#include "camera.hpp"
#include "GBuffer.hpp"

using namespace Microsoft::WRL;
class Renderer
{
public:
	Renderer(const HWND& hwnd);
	~Renderer() = default;
	Renderer(const Renderer& other) = delete;

	void draw();
	void resize();

private:
	std::chrono::system_clock::time_point m_prevTime;
	std::chrono::duration<double> m_deltaTime;
	ComPtr<ID3D11Buffer> m_constantbuffer;

	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11RenderTargetView> m_backBufferRTV;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;
	ComPtr<ID3D11Texture2D> m_backBuffer;
	ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
	glm::mat4 m_view;
	glm::mat4 m_projection;

	Camera* m_camera;

	DXDevice* m_device;
	ShaderManager* m_shaderManager;
	UIManager* m_uiManager;
	GBuffer* m_gBuffer;
};
