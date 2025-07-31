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

#include "window.hpp"
#include "camera.hpp"
#include "shaderManager.hpp"
#include "dxDevice.hpp"
#include "gltfImporter.hpp"
#include "sceneManager.hpp"
#include "uiManager.hpp"
#include "renderer.hpp"


using namespace Microsoft::WRL;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	Window window(hInstance);
	Renderer renderer(window.getHandle());	
	MSG message = {};
	while (message.message != WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		renderer.draw();
	}
	return 0;
}
