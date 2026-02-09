#include "rdocHelpers.hpp"

#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE

#include "renderdoc/renderdoc_app.h"

// g_rdocAPI is defined in renderer.cpp
extern RENDERDOC_API_1_1_2* g_rdocAPI;

RenderDocScope::RenderDocScope(ComPtr<ID3D11Device> device)
{
	if (g_rdocAPI)
	{
		g_rdocAPI->StartFrameCapture(m_device.Get(), nullptr);
	}
}

RenderDocScope::~RenderDocScope()
{
	if (g_rdocAPI)
	{
		g_rdocAPI->EndFrameCapture(nullptr, nullptr);
	}
}

#endif