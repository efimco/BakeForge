#pragma once

#include <chrono>
#include <memory>

#include <d3d11_4.h>
#include <wrl.h>

#include <glm/glm.hpp>
#include "renderdoc/renderdoc_app.h"

class Scene;
class Camera;
class DXDevice;
class ShaderManager;
class UIManager;
class ObjectPicker;
class GBuffer;
class ZPrePass;
class FSQuad;
class DeferredPass;
class CubeMapPass;
class DebugBVHPass;
class WorldSpaceUIPass;

using namespace Microsoft::WRL;

class Renderer
{
public:
	explicit Renderer(const HWND& hwnd);
	~Renderer();
	Renderer(const Renderer& other) = delete;

	void draw();
	void resize();

private:
	std::unique_ptr<Scene> m_scene;
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
	std::unique_ptr<Camera> m_camera;

	//passes
	std::unique_ptr<ObjectPicker> m_objectPicker;
	std::unique_ptr<GBuffer> m_gBuffer;
	std::unique_ptr<ZPrePass> m_zPrePass;
	std::unique_ptr<FSQuad> m_fsquad;
	std::unique_ptr<DeferredPass> m_deferredPass;
	std::unique_ptr<CubeMapPass> m_cubeMapPass;
	std::unique_ptr<DebugBVHPass> m_debugBVHPass;
	std::unique_ptr<WorldSpaceUIPass> m_worldSpaceUIPass;
	RENDERDOC_API_1_1_2* m_rdocAPI = nullptr;
};
