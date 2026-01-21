#include "uiManager.hpp"

#include <commdlg.h>
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <ranges>
#include <windows.h>

#include "imgui.h"
#define IM_VEC2_CLASS_EXTRA
#include "ImGuizmo.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "appConfig.hpp"
#include "inputEventsHandler.hpp"
#include "passes/GBuffer.hpp"

#include "baker.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "scene.hpp"
#include "texture.hpp"

#include "commands/commandManager.hpp"
#include "commands/nodeCommand.hpp"
#include "commands/nodeSnapshot.hpp"
#include "commands/scopedTransaction.hpp"


static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
static bool gUseSnap(false);
static glm::ivec3 gSnapTranslate = {1, 1, 1};
static glm::ivec3 gSnapRotate = {15, 15, 15};
static glm::ivec3 gSnapScale = {1, 1, 1};

UIManager::UIManager(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext, const HWND& hwnd)
	: m_commandManager(std::make_unique<CommandManager>())
	, m_hwnd(hwnd)
	, m_mousePos{}
	, m_scene(nullptr)
{
	m_device = device;
	m_context = deviceContext;
	m_rtvCollector = std::make_unique<RTVCollector>();
	ImGui_ImplWin32_EnableDpiAwareness();
	const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_io = &ImGui::GetIO();
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	 // Enable Gamepad Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;	 // Enable Docking
	m_io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	 // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting
	// Style + calling this again)
	style.FontScaleDpi = main_scale;
	// Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for
	// documentation purpose)
	m_io->ConfigDpiScaleFonts = true;
	// [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale
	// fonts but _NOT_ scale sizes/padding for now.
	m_io->ConfigDpiScaleViewports = true;
	// [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular
	// ones.
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

	// сonfigure ImGui multi-viewport support to use modern flip model swap chains
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		DXGI_SWAP_CHAIN_DESC flipDesc = {};
		flipDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		flipDesc.SampleDesc.Count = 1;
		flipDesc.SampleDesc.Quality = 0;
		flipDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		flipDesc.BufferCount = 2;
		flipDesc.Windowed = TRUE;
		flipDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		flipDesc.Flags = 0;

		extern void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC* desc_templates,
													 int desc_templates_count);
		ImGui_ImplDX11_SetSwapChainDescs(&flipDesc, 1);
	}
	m_isMouseInViewport = false;
}

UIManager::~UIManager()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void UIManager::draw(const ComPtr<ID3D11ShaderResourceView>& srv,
					 Scene* scene,
					 const glm::mat4& view,
					 const glm::mat4& projection)
{
	m_view = view;
	m_projection = projection;

	m_scene = scene;
	// Always check window validity before starting a new frame
	if (!IsWindow(m_hwnd) || !m_scene)
	{

		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();


	showMainMenuBar();
	showInvisibleDockWindow();
	showSceneSettings();
	showViewport(srv);
	showPassesWindow();
	drawSceneGraph();
	showProperties();
	showMaterialBrowser();
	processInputEvents();
	processNodeDeletion();
	processNodeDuplication();
	processUndoRedo();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

uint32_t* UIManager::getMousePos()
{
	return m_mousePos;
}

void UIManager::showSceneSettings() const
{

	ImGui::Begin("SceneSettings");
	ImGui::DragFloat("IBL Intensity", &AppConfig::getIBLIntensity(), 1.0f, 0.0f, 100.0f);
	ImGui::DragFloat("IBL Rotation", &AppConfig::getIBLRotation());
	ImGui::DragFloat("Environment Map Intensity", &AppConfig::getBackgroundIntensity(), 1.0f, 0.0f, 1.0f);
	ImGui::Separator();

	ImGui::Checkbox("Blur Environment Map", &AppConfig::getIsBlurred());
	if (AppConfig::getIsBlurred())
	{
		ImGui::SliderFloat("Blur Amount", &AppConfig::getBlurAmount(), 0.0f, 5.0f);
	}
	ImGui::Checkbox("Regenerate Prefiltered Map", &AppConfig::getRegeneratePrefilteredMap());
	ImGui::Separator();

	// Debug BVH visualization
	ImGui::TextWrapped("Debug Visualization");
	ImGui::Checkbox("Show Leafs only", &AppConfig::getShowLeavsOnly());
	ImGui::SliderInt("BVH Max Depth", &AppConfig::getMaxBVHDepth(), 0, 64, "%d");
	ImGui::SliderInt("BVH Min Depth", &AppConfig::getMinBVHDepth(), 0, AppConfig::getMaxBVHDepth(), "%d");


	ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_io->Framerate, m_io->Framerate);
	ImGui::End();
}

void UIManager::showMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
			{ /* TODO: New scene */
			}
			if (ImGui::MenuItem("Open...", "Ctrl+O"))
			{ /* TODO: Open file dialog */
			}
			if (ImGui::MenuItem("Save", "Ctrl+S"))
			{ /* TODO: Save scene */
			}
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
			{ /* TODO: Save as dialog */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Import Model...", "Ctrl+I"))
			{
				const std::string filepath = openFileDialog(FileType::MODEL);
				if (!filepath.empty())
				{
					m_scene->importModel(filepath, m_device);
					std::cout << "Import model triggered from menu\n";
					m_importProgress = m_scene->getImportProgress();
				}
			}
			if (ImGui::MenuItem("Export...", "Ctrl+E"))
			{ /* TODO: Export */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false))
			{ /* TODO: Undo */
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false))
			{ /* TODO: Redo */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X", false, false))
			{ /* TODO: Cut */
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, false))
			{ /* TODO: Copy */
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, false))
			{ /* TODO: Paste */
			}
			if (ImGui::MenuItem("Delete", "Del"))
			{ /* TODO: Delete selected */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A"))
			{ /* TODO: Select all */
			}
			if (ImGui::MenuItem("Deselect All", "Ctrl+D"))
			{
				m_scene->clearSelectedNodes();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Viewport", nullptr, true))
			{ /* Toggle viewport */
			}
			if (ImGui::MenuItem("Scene Graph", nullptr, true))
			{ /* Toggle scene graph */
			}
			if (ImGui::MenuItem("Properties", nullptr, true))
			{ /* Toggle properties */
			}
			if (ImGui::MenuItem("Material Browser", nullptr, true))
			{ /* Toggle material browser */
			}
			if (ImGui::MenuItem("GBuffer", nullptr, true))
			{ /* Toggle GBuffer view */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{ /* TODO: Reset docking layout */
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::BeginMenu("Mesh"))
			{
				if (ImGui::MenuItem("Cube"))
				{ /* TODO: Add cube */
				}
				if (ImGui::MenuItem("Sphere"))
				{ /* TODO: Add sphere */
				}
				if (ImGui::MenuItem("Plane"))
				{ /* TODO: Add plane */
				}
				if (ImGui::MenuItem("Cylinder"))
				{ /* TODO: Add cylinder */
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Light"))
			{
				auto lightPos = glm::vec3(0.0f, 1.0f, 0.0f);
				if (m_scene->getLights().size() > 0)
				{
					const auto lastAddedLight = std::prev(m_scene->getLights().end());
					lightPos = lastAddedLight->second->transform.position + glm::vec3(1.0f, 0.0f, 0.0f);
				}

				if (ImGui::MenuItem("Point Light"))
				{
					auto pointLight = std::make_unique<Light>(POINT_LIGHT, lightPos, "Point Light");
					m_scene->addChild(std::move(pointLight));
				}
				if (ImGui::MenuItem("Directional Light"))
				{
					auto dirLight = std::make_unique<Light>(DIRECTIONAL_LIGHT, lightPos, "Directional Light");
					m_scene->addChild(std::move(dirLight));
				}
				if (ImGui::MenuItem("Spot Light"))
				{
					auto spotLight = std::make_unique<Light>(SPOT_LIGHT, lightPos, "Spot Light");
					m_scene->addChild(std::move(spotLight));
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Empty Node"))
			{ /* TODO: Add empty node */
			}
			if (ImGui::MenuItem("Baker"))
			{
				auto baker = std::make_unique<Baker>("Baker");
				m_scene->addChild(std::move(baker));
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render"))
		{
			if (ImGui::MenuItem("Render Image"))
			{ /* TODO: Render to file */
			}
			ImGui::Separator();
			if (ImGui::BeginMenu("Debug Views"))
			{
				if (ImGui::MenuItem("Albedo"))
				{ /* TODO: Show albedo only */
				}
				if (ImGui::MenuItem("Normals"))
				{ /* TODO: Show normals */
				}
				if (ImGui::MenuItem("Metallic/Roughness"))
				{ /* TODO: Show metallic/roughness */
				}
				if (ImGui::MenuItem("Depth"))
				{ /* TODO: Show depth */
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
			{ /* TODO: Show about dialog */
			}
			if (ImGui::MenuItem("Documentation"))
			{ /* TODO: Open docs */
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void UIManager::showViewport(const ComPtr<ID3D11ShaderResourceView>& srv)
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("Viewport", nullptr, windowFlags);
	m_isMouseInViewport = ImGui::IsWindowHovered();


	constexpr auto uv0 = ImVec2(0, 0);
	constexpr auto uv1 = ImVec2(1, 1);
	static ImVec2 prevSize = ImGui::GetContentRegionAvail();
	ImVec2 size = ImGui::GetContentRegionAvail();
	size.x = static_cast<int>(size.x) % 2 == 0 ? size.x : size.x - 1;
	size.y = static_cast<int>(size.y) % 2 == 0 ? size.y : size.y - 1;

	AppConfig::setViewportWidth(static_cast<int>(size.x));
	AppConfig::setViewportHeight(static_cast<int>(size.y));

	if (size.x != prevSize.x || size.y != prevSize.y)
	{
		AppConfig::setNeedsResize(true);
		prevSize = size;
	}

	const ImTextureRef tex = srv.Get();
	const auto mousePos =
		ImVec2(m_io->MousePos.x - ImGui::GetWindowPos().x, m_io->MousePos.y - ImGui::GetWindowPos().y);
	m_mousePos[0] = static_cast<uint32_t>(mousePos.x);
	m_mousePos[1] = static_cast<uint32_t>(mousePos.y);

	// Set up ImGuizmo to draw in this viewport
	const ImVec2 windowPos = ImGui::GetWindowPos();
	ImGuizmo::SetRect(windowPos.x, windowPos.y, size.x, size.y);

	ImGui::Image(tex, size, uv0, uv1);

	showChWSnappingOptions();
	showChWViewportOptions();
	if (m_importProgress)
	{
		showChWImportProgress(m_importProgress);
		if (m_importProgress->isCompleted.load())
		{
			m_importProgress = nullptr;
		}
	}


	processGizmo();

	ImGui::End();
	ImGui::PopStyleVar(3);
}


void UIManager::showChWSnappingOptions()
{
	// Snap toggle button in top right corner of viewport
	// Gizmo controls overlay
	ImGui::SetCursorPos(ImVec2(5.0f, 5.0f));
	ImGui::BeginChild("Snapping Options", ImVec2(200.0f, 150.0f), true, ImGuiWindowFlags_NoScrollbar);
	{
		if (ImGui::Button(gUseSnap ? "Snap: On" : "Snap: Off", ImVec2(-1.0f, 20.0f)))
		{
			gUseSnap = !gUseSnap;
		}

		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;

		ImGui::PushItemWidth(-1.0f);
		ImGui::Text("[T]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapTranslate", &gSnapTranslate[0], 0, 100);
		ImGui::Text("[R]");
		ImGui::SameLine();
		;
		ImGui::SliderInt("##SnapRotate", &gSnapRotate[0], 1, 180);
		ImGui::Text("[S]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapScale", &gSnapScale[0], 0, 100);
		ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}

void UIManager::showChWViewportOptions()
{
	// Viewport options child window
	ImGui::SetCursorPos(ImVec2(AppConfig::getViewportWidth() - 205.0f, 5.0f));
	ImGui::BeginChild("ViewportOptions", ImVec2(200.0f, 150.0f), false, ImGuiWindowFlags_NoScrollbar);
	{
		if (ImGui::RadioButton("Show UI Overlay", AppConfig::getDrawWSUI()))
		{
			AppConfig::getDrawWSUI() = !AppConfig::getDrawWSUI();
		}
	}
	ImGui::EndChild();
}

void UIManager::showChWImportProgress(std::shared_ptr<ImportProgress> progress)
{
	// Viewport options child window
	ImGui::SetCursorPos(ImVec2(AppConfig::getViewportWidth() - 300.0f, AppConfig::getViewportHeight() - 100.0f));
	ImGui::BeginChild("ImportProgress", ImVec2(250.0f, 150.0f), false, ImGuiWindowFlags_NoScrollbar);
	{
		ImGui::Text("Import Progress:");
		ImGui::ProgressBar(progress->progress.load(), ImVec2(-1.0f, 0.0f));
		ImGui::Text("%s", progress->getStage().c_str());
	}
	ImGui::EndChild();
}

void UIManager::showInvisibleDockWindow()
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("InvisibleDockSpaceWindow", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

void UIManager::showMaterialBrowser()
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
	ImGui::Begin("Material Browser", nullptr, windowFlags);
	const std::vector<std::string> materialNames = m_scene->getMaterialNames();
	const ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	constexpr float maxItemSize = 100.0f;
	const int itemsPerRow = static_cast<int>(floor(contentRegion.x / (maxItemSize + ImGui::GetStyle().ItemSpacing.x)));
	const float itemSize = (contentRegion.x - (itemsPerRow)*ImGui::GetStyle().ItemSpacing.x * 2) / itemsPerRow;
	for (int i = 0; i < materialNames.size(); i++)
	{
		const auto mat = m_scene->getMaterial(materialNames[i]);
		const ImTextureRef previewTexture = mat->preview.srv_preview.Get();
		if (ImGui::ImageButton(materialNames[i].c_str(), previewTexture, ImVec2(itemSize, itemSize)))
		{
			m_scene->setActiveNode(nullptr);
			m_selectedMaterial = mat;
		}
		if (itemsPerRow > 0 && (i + 1) % itemsPerRow != 0 && i < materialNames.size() - 1)
		{
			ImGui::SameLine();
		}
	}
	ImGui::End();
}

void UIManager::processInputEvents() const
{
	InputEvents::setMouseClicked(MouseButtons::LEFT_BUTTON, m_io->MouseClicked[0]);
	InputEvents::setMouseClicked(MouseButtons::MIDDLE_BUTTON, m_io->MouseClicked[2]);
	InputEvents::setMouseClicked(MouseButtons::RIGHT_BUTTON, m_io->MouseClicked[1]);

	InputEvents::setMouseDown(MouseButtons::LEFT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Left));
	InputEvents::setMouseDown(MouseButtons::MIDDLE_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Middle));
	InputEvents::setMouseDown(MouseButtons::RIGHT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Right));

	InputEvents::setMouseInViewport(m_isMouseInViewport);

	InputEvents::setMouseDelta(m_io->MouseDelta.x, m_io->MouseDelta.y);
	InputEvents::setMouseWheel(m_io->MouseWheel);

	InputEvents::setKeyDown(KeyButtons::KEY_W, ImGui::IsKeyDown(ImGuiKey_W));
	InputEvents::setKeyDown(KeyButtons::KEY_A, ImGui::IsKeyDown(ImGuiKey_A));
	InputEvents::setKeyDown(KeyButtons::KEY_S, ImGui::IsKeyDown(ImGuiKey_S));
	InputEvents::setKeyDown(KeyButtons::KEY_D, ImGui::IsKeyDown(ImGuiKey_D));
	InputEvents::setKeyDown(KeyButtons::KEY_Q, ImGui::IsKeyDown(ImGuiKey_Q));
	InputEvents::setKeyDown(KeyButtons::KEY_E, ImGui::IsKeyDown(ImGuiKey_E));
	InputEvents::setKeyDown(KeyButtons::KEY_F, ImGui::IsKeyDown(ImGuiKey_F));
	InputEvents::setKeyDown(KeyButtons::KEY_LSHIFT, ImGui::IsKeyDown(ImGuiKey_LeftShift));
	InputEvents::setKeyDown(KeyButtons::KEY_F12, ImGui::IsKeyDown(ImGuiKey_F12));
	InputEvents::setKeyDown(KeyButtons::KEY_F11, ImGui::IsKeyPressed(ImGuiKey_F11, false));
}

void UIManager::processGizmo()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode == nullptr)
		return;
	if (!activeNode->movable)
		return;
	ImGuizmo::SetDrawlist();
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_G))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) // r Key
			mCurrentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_M))
			gUseSnap = !gUseSnap;
	}

	TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{m_commandManager.get(), m_scene, activeNode};

	glm::mat4 worldMatrix = activeNode->getWorldMatrix();
	const auto matrix = glm::value_ptr(worldMatrix);
	const auto viewMatrix = const_cast<float*>(glm::value_ptr(m_view));
	const auto projectionMatrix = const_cast<float*>(glm::value_ptr(m_projection));
	glm::vec3 currentSnapValue;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		currentSnapValue = gSnapTranslate;
		break;
	case ImGuizmo::ROTATE:
		currentSnapValue = glm::vec3(gSnapRotate);
		break;
	case ImGuizmo::SCALE:
		currentSnapValue = gSnapScale;
		break;
	default:
		currentSnapValue = glm::vec3(0.0f);
		break;
	}
	ImGuizmo::Manipulate(viewMatrix, projectionMatrix, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, nullptr,
						 gUseSnap ? &currentSnapValue[0] : nullptr);
	if (ImGuizmo::IsUsing())
	{
		m_isMouseInViewport = false;

		// Convert the modified world matrix back to local space
		glm::mat4 newWorldMatrix = worldMatrix; // matrix was modified in-place
		glm::mat4 localMatrix;
		if (activeNode->parent && !dynamic_cast<Scene*>(activeNode->parent))
		{
			// Has a non-Scene parent: convert world -> local
			const glm::mat4 parentWorldInverse = glm::inverse(activeNode->parent->getWorldMatrix());
			localMatrix = parentWorldInverse * newWorldMatrix;
		}
		else
		{
			// No parent or parent is Scene: world == local
			localMatrix = newWorldMatrix;
		}
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation,
											  matrixScale);
		activeNode->transform.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
		activeNode->transform.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
		activeNode->transform.scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
		if (gUseSnap && mCurrentGizmoOperation == ImGuizmo::TRANSLATE)
		{
			ImGuizmo::DrawGrid(viewMatrix, projectionMatrix, glm::value_ptr(newWorldMatrix), 100.0f);
		}
		if (dynamic_cast<Primitive*>(activeNode))
		{
		}
		if (dynamic_cast<Light*>(activeNode))
		{
			m_scene->setLightsDirty();
		}
	}
}

void UIManager::processNodeDuplication()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode && ImGui::IsKeyPressed(ImGuiKey_D, false) && InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		auto createSceneNode = std::make_unique<Command::DuplicateSceneNode>(m_scene, activeNode);
		m_commandManager->commitCommand(std::move(createSceneNode));
	}
}

void UIManager::processNodeDeletion()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		const bool isPrimitive = dynamic_cast<Primitive*>(activeNode) != nullptr;
		const bool isLight = dynamic_cast<Light*>(activeNode) != nullptr;
		auto removeSceneNode = std::make_unique<Command::RemoveSceneNode>(m_scene, activeNode);
		m_commandManager->commitCommand(std::move(removeSceneNode));
		if (isPrimitive)
		{
		}
		if (isLight)
		{
			m_scene->setLightsDirty();
		}
	}
}

void UIManager::processUndoRedo() const
{
	// We put a merge fence when mouse is released
	const bool isMouseReleased =
		ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right);
	if (isMouseReleased)
	{
		m_commandManager->setMergeFence();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (ImGui::IsKeyDown(ImGuiMod_Shift))
		{
			if (m_commandManager->hasRedoCommands())
			{
				m_commandManager->redo();
			}
		}
		else
		{
			if (m_commandManager->hasUndoCommands())
			{
				m_commandManager->undo();
			}
		}
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (m_commandManager->hasRedoCommands())
		{
			m_commandManager->redo();
		}
	}
}

void UIManager::drawSceneGraph()
{
	ImGui::Begin("Scene Graph");
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

	const ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (ImGui::BeginTable("SceneGraph", 1, tableFlags, availableSize))
	{
		ImGui::TableSetupColumn(m_scene->name.c_str(), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		drawNode(m_scene);
		ImGui::EndTable();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
			{
				SceneNode* draggedNode = *static_cast<SceneNode**>(payload->Data);
				if (draggedNode && draggedNode->parent)
				{
					auto reparentSceneNode =
						std::make_unique<Command::ReparentSceneNode>(m_scene, draggedNode, m_scene);
					m_commandManager->commitCommand(std::move(reparentSceneNode));
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::End();
}

void UIManager::handleNodeSelection(SceneNode* node)
{
	bool isShiftPressed = InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT);
	float readBackID = m_scene->getReadBackID();
	if (ImGui::IsItemClicked())
	{
		m_scene->setActiveNode(node, isShiftPressed);
		m_selectedMaterial = nullptr;
		m_scene->setReadBackID(-1.0f);
	}
	else if (readBackID == 0)
	{
		m_scene->clearSelectedNodes();
	}
	else if (readBackID > 0)
	{
		auto* selectedNode = m_scene->getNodeByHandle(SceneNodeHandle(static_cast<int32_t>(readBackID)));
		if (!selectedNode)
			return;
		m_scene->setActiveNode(selectedNode, isShiftPressed);
		m_selectedMaterial = nullptr;
	}
}

void UIManager::handleNodeDragDrop(SceneNode* node)
{
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_NODE", &node, sizeof(SceneNode*));
		ImGui::Text("%s", node->name.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
		{
			SceneNode* draggedNode = *static_cast<SceneNode**>(payload->Data);
			if (draggedNode && draggedNode != node && draggedNode->parent)
			{
				auto reparentSceneNode = std::make_unique<Command::ReparentSceneNode>(m_scene, draggedNode, node);
				m_commandManager->commitCommand(std::move(reparentSceneNode));
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void UIManager::drawNode(SceneNode* node)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	const bool isScene = dynamic_cast<Scene*>(node);
	if (isScene)
	{
		for (auto& child : node->children)
		{
			drawNode(child.get());
		}
		return;
	}
	const auto baker = dynamic_cast<Baker*>(node);
	const bool isFolder = (node->children.size() > 0);
	const char* icon = getNodeIcon(node);
	const std::string label = std::string(icon) + " " + node->name;
	if (isFolder || baker)
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		const bool isSelected = m_scene->isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		const bool open = ImGui::TreeNodeEx(label.c_str(), nodeFlags);
		handleNodeSelection(node);
		handleNodeDragDrop(node);

		if (open)
		{
			for (auto& child : node->children)
			{
				drawNode(child.get());
			}
			if (baker)
			{
				drawNode(baker->lowPoly.get());
				drawNode(baker->highPoly.get());
			}
			ImGui::TreePop();
		}

		ImGui::TableNextColumn();
	}
	else
	{
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		const bool isSelected = m_scene->isNodeSelected(node);
		if (isSelected)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}
		ImGui::TreeNodeEx(label.c_str(), nodeFlags);

		handleNodeDragDrop(node);
		handleNodeSelection(node);
		ImGui::TableNextColumn();
	}
}

const char* UIManager::getNodeIcon(SceneNode* node)
{
	if (dynamic_cast<Light*>(node))
	{
		return "[L]"; // Light
	}
	else if (dynamic_cast<Primitive*>(node))
	{
		return "[P]"; // Primitive
	}
	else if (dynamic_cast<Camera*>(node))
	{
		return "[C]"; // Camera
	}
	else if (dynamic_cast<Baker*>(node))
	{
		return "[BKR]";
	}
	else if (dynamic_cast<LowPolyNode*>(node))
	{
		return "[LP]";
	}
	else if (dynamic_cast<HighPolyNode*>(node))
	{
		return "[HP]";
	}
	else if (!node->children.empty())
	{
		return "[SN]"; // Folder
	}

	return "[D]"; // Default document icon
}

void UIManager::showPassesWindow()
{
	ImGui::Begin("Passes");
	auto& rtvMap = m_rtvCollector->getRTVMap();

	static std::vector<std::string> rtvNameStrings;
	static std::vector<const char*> rtvNames;
	static size_t lastMapSize = 0;

	// Only rebuild when map size changes
	if (rtvMap.size() != lastMapSize)
	{
		rtvNameStrings.clear();
		rtvNames.clear();
		rtvNameStrings.reserve(rtvMap.size());
		rtvNames.reserve(rtvMap.size());
		for (const auto& name : rtvMap | std::views::keys)
		{
			rtvNameStrings.push_back(name);
		}
		for (const auto& name : rtvNameStrings)
		{
			rtvNames.push_back(name.c_str());
		}
		lastMapSize = rtvMap.size();
	}

	static int currentItemIndex = 0;
	if (rtvNames.empty())
	{
		ImGui::End();
		return;
	}

	if (currentItemIndex >= static_cast<int>(rtvNames.size()))
		currentItemIndex = 0;

	ImGui::Combo("RTVList", &currentItemIndex, rtvNames.data(), static_cast<int>(rtvNames.size()));

	const ImTextureRef tex = rtvMap[rtvNameStrings[currentItemIndex]];
	constexpr auto uv0 = ImVec2(0, 0);
	constexpr auto uv1 = ImVec2(1, 1);
	ImVec2 size = ImGui::GetContentRegionAvail();
	float aspectRatio = AppConfig::getAspectRatio();
	size.x = size.y * aspectRatio;
	ImGui::Image(tex, size, uv0, uv1);
	ImGui::End();
}

void UIManager::showProperties() const
{
	ImGui::Begin("Properties");
	if (m_selectedMaterial)
	{
		showMaterialProperties(m_selectedMaterial);
		ImGui::End();
		return;
	}
	else if (m_scene->getActiveNode())
	{
		if (dynamic_cast<Primitive*>(m_scene->getActiveNode()))
		{
			showPrimitiveProperties(dynamic_cast<Primitive*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Light*>(m_scene->getActiveNode()))
		{
			showLightProperties(dynamic_cast<Light*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Camera*>(m_scene->getActiveNode()))
		{
			showCameraProperties(dynamic_cast<Camera*>(m_scene->getActiveNode()));
		}
		ImGui::Text("Selected Node: %s", m_scene->getActiveNode()->name.c_str());
	}
	ImGui::End();
}

void UIManager::showPrimitiveProperties(Primitive* primitive) const
{
	{
		TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{m_commandManager.get(), m_scene, primitive};

		ImGui::Text("Type: Primitive");
		ImGui::Text("Name: %s", primitive->name.c_str());
		ImGui::DragFloat3("Position", &primitive->transform.position[0], 0.1f);
		ImGui::DragFloat3("Rotation", &primitive->transform.rotation[0], 0.1f);
		ImGui::DragFloat3("Scale", &primitive->transform.scale[0], 0.1f);
	}

	{
		const auto& materialNames = m_scene->getMaterialNames();
		int currentMaterialIndex = -1;
		// Find current material index
		if (currentMaterialIndex < 0 && primitive->material)
		{
			int index = 0;
			for (const auto& name : materialNames)
			{
				if (m_scene->getMaterial(name) == primitive->material)
				{
					currentMaterialIndex = index;
					break;
				}
				index++;
			}
		}

		// Save previous material index
		const int previousMaterialIndex = currentMaterialIndex;
		if (ImGui::BeginCombo("Material",
							  currentMaterialIndex >= 0 && currentMaterialIndex < materialNames.size()
								  ? materialNames[currentMaterialIndex].c_str()
								  : "Select Material"))
		{
			for (int n = 0; n < static_cast<int>(materialNames.size()); n++)
			{
				const bool isSelected = (currentMaterialIndex == n);
				if (ImGui::Selectable(materialNames[n].c_str(), isSelected))
				{
					currentMaterialIndex = n;
					if (currentMaterialIndex != previousMaterialIndex)
					{
						TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{m_commandManager.get(), m_scene,
																					primitive};
						const std::shared_ptr<Material> selectedMaterial = m_scene->getMaterial(materialNames[n]);
						primitive->material = selectedMaterial;
					}
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
}

void UIManager::showMaterialProperties(std::shared_ptr<Material> material)
{
	ImGui::Text("Name: %s", material->name.c_str());
	ImGui::Text("Albedo: ");
	ImGui::SameLine();
	ImGui::Image(material->albedo->srv.Get(), ImVec2(128, 128));
	ImGui::ColorEdit4("Albedo color: ", glm::value_ptr(material->albedoColor));
	ImGui::Text("MetallicRougness: ");
	ImGui::SameLine();
	ImGui::Image(material->metallicRoughness->srv.Get(), ImVec2(128, 128));
	ImGui::DragFloat("Metallic", &material->metallicValue, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Roughness", &material->roughnessValue, 0.01f, 0.04f, 1.0f);
	ImGui::Text("Normal: ");
	ImGui::SameLine();
	ImGui::Image(material->normal->srv.Get(), ImVec2(128, 128));
}

void UIManager::showLightProperties(Light* light) const
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{m_commandManager.get(), m_scene, light};

	ImGui::Text("Name: %s", light->name.c_str());

	ImGui::Combo("Type", reinterpret_cast<int*>(&light->type), "Point Light\0Directional Light\0Spot Light\0");
	ImGui::DragFloat3("Position", &light->transform.position[0], 0.1f);
	ImGui::DragFloat3("Rotation", &light->transform.rotation[0], 0.1f);
	ImGui::ColorEdit3("Color", &light->color[0]);
	ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 100.0f);
}


void UIManager::showCameraProperties(Camera* camera) const
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{m_commandManager.get(), m_scene, camera};

	ImGui::Text("Name: %s", camera->name.c_str());
	ImGui::DragFloat3("Position", &camera->orbitPivot[0], 0.1f);
	ImGui::DragFloat3("Rotation", &camera->transform.rotation[0], 0.1f);
	ImGui::DragFloat("Fov", &camera->fov, 0.1f, 1.0f, 120.0f);
}

std::string UIManager::openFileDialog(const FileType outFileType)
{
	OPENFILENAME ofn;
	char fileName[260] = {0};
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = sizeof(fileName);

	if (outFileType == FileType::IMAGE)
	{
		ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.hdr;*.exr\0All Files\0*.*\0";
	}
	else if (outFileType == FileType::MODEL)
	{
		ofn.lpstrFilter = "Model Files\0*.gltf;*.glb;*.obj\0All Files\0*.*\0";
	}
	else
	{
		ofn.lpstrFilter = "All Files\0*.*\0";
	}
	ofn.nFilterIndex = 1;
	ofn.lpstrTitle = "Select a Texture";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	// Open the dialog box
	if (GetOpenFileNameA(&ofn) == TRUE)
	{
		return std::string(ofn.lpstrFile);
	}

	return std::string();
}
