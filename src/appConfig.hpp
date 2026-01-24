#pragma once

namespace AppConfig
{
	inline int windowWidth = 2;
	inline int windowHeight = 2;
	inline int viewportWidth = 2;
	inline int viewportHeight = 2;

	inline bool needsResize = true;
	inline float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	inline bool isWindowMinimized = false;
	inline double deltaTime = 0.0;
	inline float IBLintensity = 1.0f;
	inline float IBLrotation = 0.0f;
	inline bool regeneratePrefilteredMap = false;
	inline bool isBackgroundBlurred = false;
	inline float blurAmount = 0.5f;
	inline float backgroundIntensity = 1.0f;

	inline bool drawWSUI = true;
	inline bool captureNextFrame = false;

	inline int maxBVHDepth = 1;
	inline int minBVHDepth = 0;
	inline bool showLeafsOnly = false;

	inline bool bake = false;

	inline float rayLength = 0.01f;

	inline float getAspectRatio()
	{
		return static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
	}
} // namespace AppConfig
