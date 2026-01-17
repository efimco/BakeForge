
#pragma once
#include <memory>
#include <string>

#include <d3d11_4.h>
#include <wrl.h>

#include <glm/glm.hpp>
#include "imgui.h"
#include "rtvCollector.hpp"

class Scene;
class SceneNode;
class Camera;
class Primitive;
struct Material;
class Light;
class CommandManager;
class RTVCollector;


#define ICON_FA_CUBE "\xef\x86\xb2"		 // Mesh/Primitive
#define ICON_FA_LIGHTBULB "\xef\x83\xab" // Light
#define ICON_FA_FOLDER "\xef\x81\xbb"	 // Folder/Group
#define ICON_FA_CAMERA "\xef\x80\xbd"	 // Camera
#define ICON_FA_IMAGE "\xef\x80\xbe"	 // Material/Texture

using namespace Microsoft::WRL;

struct ImportProgress;

enum class FileType
{
	IMAGE,
	MODEL,
	UNKNOWN
};

class UIManager
{
public:
	explicit UIManager(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext, const HWND& hwnd);
	~UIManager();

	void
	draw(const ComPtr<ID3D11ShaderResourceView>& srv, Scene* scene, const glm::mat4& view, const glm::mat4& projection);
	uint32_t* getMousePos();

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	std::unique_ptr<CommandManager> m_commandManager;
	std::unique_ptr<RTVCollector> m_rtvCollector;
	const HWND& m_hwnd;
	ImGuiIO* m_io;
	uint32_t m_mousePos[2];
	bool m_isMouseInViewport;
	Scene* m_scene;
	std::shared_ptr<Material> m_selectedMaterial = nullptr;
	void showSceneSettings() const;
	void showMainMenuBar();
	void showViewport(const ComPtr<ID3D11ShaderResourceView>& srv);
	static void showChWSnappingOptions(); // ChW stands for "Child Window"
	static void showChWViewportOptions();
	static void showChWImportProgress(std::shared_ptr<ImportProgress> progress);
	static void showInvisibleDockWindow();
	void showMaterialBrowser();

	void processInputEvents() const;
	void processGizmo();
	void processNodeDuplication();
	void processNodeDeletion();
	void processUndoRedo() const;

	void drawSceneGraph();
	void handleNodeSelection(SceneNode* node);
	void handleNodeDragDrop(SceneNode* node);
	void drawNode(SceneNode* node);
	void showPassesWindow();
	static const char* getNodeIcon(SceneNode* node);

	void showProperties() const;
	void showPrimitiveProperties(Primitive* primitive) const;
	static void showMaterialProperties(std::shared_ptr<Material> material);
	void showLightProperties(Light* light) const;
	void showCameraProperties(Camera* camera) const;

	std::shared_ptr<ImportProgress> m_importProgress = nullptr;

	static std::string openFileDialog(FileType outFileType);

	glm::mat4 m_view;
	glm::mat4 m_projection;
};
