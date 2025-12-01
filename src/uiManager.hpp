#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "imgui.h"
#include "GBuffer.hpp"
#include "scene.hpp"
#include "light.hpp"

#define ICON_FA_CUBE       "\xef\x86\xb2"     // Mesh/Primitive
#define ICON_FA_LIGHTBULB  "\xef\x83\xab"     // Light
#define ICON_FA_FOLDER     "\xef\x81\xbb"     // Folder/Group
#define ICON_FA_CAMERA     "\xef\x80\xbd"     // Camera
#define ICON_FA_IMAGE      "\xef\x80\xbe"     // Material/Texture

using namespace Microsoft::WRL;

class UIManager
{
public:
	UIManager(const ComPtr<ID3D11Device>& device,
		const ComPtr<ID3D11DeviceContext>& deviceContext,
		const HWND& hwnd);
	~UIManager();

	void draw(const ComPtr<ID3D11ShaderResourceView>& srv, const GBuffer& gbuffer, Scene* scene);
	uint32_t* getMousePos();

private:
	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;
	const HWND& m_hwnd;
	ImGuiIO* m_io;
	ImFont* m_iconFont = nullptr;
	uint32_t m_mousePos[2];
	bool m_isMouseInViewport;
	Scene* m_scene;
	void simpleWindow();
	void showMainMenuBar();
	void showViewport(const ComPtr<ID3D11ShaderResourceView>& srv);
	void showInvisibleDockWindow();
	void showMaterialBrowser();
	void showGBufferImage(const GBuffer& gbuffer);
	void processInputEvents();

	void drawSceneGraph();
	void handleNodeSelection(SceneNode* node);
	void handleNodeDragDrop(SceneNode* node);
	void drawNode(SceneNode* node);
	const char* getNodeIcon(SceneNode* node);

	void showProperties();
	void showPrimitiveProperties(Primitive* primitive);
	void showMaterialProperties(std::shared_ptr<Material> material);
	void showLightProperties(Light* light);
	void showCameraProperties(Camera* camera);

};
