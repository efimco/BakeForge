#include "appConfig.hpp"

static int windowWidth = 2;
static int windowHeight = 2;
static int viewportWidth = 2;
static int viewportHeight = 2;
static bool needsResize = true;
static float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
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

bool& AppConfig::getNeedsResize()
{
	return needsResize;
}

void AppConfig::setNeedsResize(bool _needsResize)
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

void AppConfig::setWindowHeight(int height)
{
	windowHeight = height;
}

void AppConfig::setWindowMinimized(bool _isMnimized)
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

bool& AppConfig::getShowBVH()
{
	return showBVH;
}

bool& AppConfig::getShowPrimitiveBVH()
{
	return showPrimitiveBVH;
}

int& AppConfig::getBVHMaxDepth()
{
	return bvhMaxDepth;
}

void AppConfig::setDeltaTime(double value)
{
	deltaTime = value;
}
double AppConfig::getDeltaTime()
{
	return deltaTime;
}

void AppConfig::setWindowWidth(int width)
{
	windowWidth = width;
}

void AppConfig::setViewportHeight(int height)
{
	viewportHeight = height;
}

void AppConfig::setViewportWidth(int width)
{
	viewportWidth = width;
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
void AppConfig::setClearColor(float r, float g, float b, float a)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
	clearColor[3] = a;
}
