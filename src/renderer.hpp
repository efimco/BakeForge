#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "dxDevice.hpp"
#include "shaderManager.hpp"
#include "uiManager.hpp"
#include "camera.hpp"
#include "GBuffer.hpp"
#include "FSQuad.hpp"
#include "objectPicker.hpp"
#include "ZPrePass.hpp"
#include "deferedPass.hpp"

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
	SceneNode* m_scene;
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

	std::unique_ptr<DXDevice> m_device;
	std::unique_ptr<ShaderManager> m_shaderManager;
	std::unique_ptr<UIManager> m_uiManager;
	std::unique_ptr<ObjectPicker> m_objectPicker;
	std::unique_ptr<Camera> m_camera;
	std::unique_ptr<GBuffer> m_gBuffer;
	std::unique_ptr<ZPrePass> m_zPrePass;
	std::unique_ptr<FSQuad> m_fsquad;
	std::unique_ptr<DeferredPass> m_deferredPass;
};
