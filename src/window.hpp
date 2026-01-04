#pragma once

#include <windows.h>
#include <d3d11_4.h>

class Window
{
public:
	Window(HINSTANCE hInstance);
	~Window() = default;

	const HWND& getHandle() const;


private:
	HWND m_hWindow;
	WNDCLASS m_windClass;
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};