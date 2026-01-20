#include "appConfig.hpp"
static int windowWidth = 2;
static int windowHeight = 2;
static int viewportWidth = 2;
static int viewportHeight = 2;
static bool needsResize = true;
static float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool isMinimized = false;
static double deltaTime = 0.0;
static float IBLintensity = 1.0f;
static float IBLrotation = 0.0f;
static bool regeneratePrefilteredMap = false;
static bool showBVH = false;
static bool showPrimitiveBVH = false;
static int bvhMaxDepth = -1;
static bool isBackgroundBlurred = false;
static float blurAmount = 0.5f;
static float backgroundIntensity = 1.0f;
static bool drawWSUI = true;
static bool captureNextFrame = false;
static int maxBVHDepth = 1;
static int minBVHDepth = 0;
static bool showLeafsOnly = false;


bool& AppConfig::getNeedsResize()
{
	return needsResize;
}

void AppConfig::setNeedsResize(const bool _needsResize)
{
	needsResize = _needsResize;
}

const int& AppConfig::getWindowWidth()
{
	return windowWidth;
}

const int& AppConfig::getWindowHeight()
{
	return windowHeight;
}

const int& AppConfig::getViewportWidth()
{
	return viewportWidth;
}

const int& AppConfig::getViewportHeight()
{
	return viewportHeight;
}

float AppConfig::getAspectRatio()
{
	return static_cast<float>(viewportWidth) / viewportHeight;
}

void AppConfig::setViewportWidth(const int width)
{
	viewportWidth = width;
}

void AppConfig::setViewportHeight(const int height)
{
	viewportHeight = height;
}

void AppConfig::setWindowWidth(const int width)
{
	windowWidth = width;
}

void AppConfig::setWindowHeight(const int height)
{
	windowHeight = height;
}

void AppConfig::setWindowMinimized(const bool _isMnimized)
{
	isMinimized = _isMnimized;
}

float& AppConfig::getIBLIntensity()
{
	return IBLintensity;
}

float& AppConfig::getIBLRotation()
{
	return IBLrotation;
}

float& AppConfig::getBackgroundIntensity()
{
	return backgroundIntensity;
}

bool& AppConfig::getRegeneratePrefilteredMap()
{
	return regeneratePrefilteredMap;
}

bool& AppConfig::getShowLeavsOnly()
{
	return showLeafsOnly;
}

int& AppConfig::getMaxBVHDepth()
{
	return maxBVHDepth;
}

int& AppConfig::getMinBVHDepth()
{
	return minBVHDepth;
}

void AppConfig::setDeltaTime(const double value)
{
	deltaTime = value;
}

double AppConfig::getDeltaTime()
{
	return deltaTime;
}

float* AppConfig::getClearColor()
{
	return clearColor;
}

bool& AppConfig::getIsBlurred()
{
	return isBackgroundBlurred;
}

float& AppConfig::getBlurAmount()
{
	return blurAmount;
}

void AppConfig::setClearColor(const float r, const float g, float b, float a)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
	clearColor[3] = a;
}

bool& AppConfig::getDrawWSUI()
{
	return drawWSUI;
}

void AppConfig::setCaptureNextFrame(const bool capture)
{
	captureNextFrame = capture;
}

bool AppConfig::getCaptureNextFrame()
{
	return captureNextFrame;
}
