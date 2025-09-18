#ifndef UNICODE
#define UNICODE
#endif
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <assert.h>
#include <chrono>
#include <ctime>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <vector>
#include <wrl.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <windows.h>
#include <ShellScalingAPI.h>

// No manifest approach: set process DPI awareness at runtime
// Tries Per-Monitor v2 first, then Per-Monitor, then System-aware as last resort.
static void SetHighDpiAwarenessAtRuntime()
{
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	if (user32)
	{
		typedef BOOL(WINAPI* SetProcessDpiAwarenessContext_t)(HANDLE);
		auto pSetProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContext_t>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

		// Define constant if headers are older
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

		if (pSetProcessDpiAwarenessContext)
		{
			if (pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
				return; // Success
		}
	}
}

#include "window.hpp"
#include "camera.hpp"
#include "dxDevice.hpp"
#include "gltfImporter.hpp"
#include "sceneManager.hpp"
#include "uiManager.hpp"
#include "renderer.hpp"


using namespace Microsoft::WRL;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	// Ensure high-DPI awareness without a manifest, before any window is created
	SetHighDpiAwarenessAtRuntime();

	Window window(hInstance);
	Renderer renderer(window.getHandle());
	MSG message = {};
	bool running = true;
	while (running)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
			{
				running = false;
				break;
			}

			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (running)
			renderer.draw();
	}
	return 0;
}
