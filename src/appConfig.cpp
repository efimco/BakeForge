#include "appConfig.hpp"

static int windowWidth = 2;
static int windowHeight = 2;
static int viewportWidth = 2;
static int viewportHeight = 2;
static bool needsResize = true;
static float clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f };
static bool isMinimized = false;
static double deltaTime = 0.0;


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
void AppConfig::setClearColor(float r, float g, float b, float a)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
	clearColor[3] = a;
}
