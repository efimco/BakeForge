#include "appConfig.hpp"

static int windowWidth = 2;
static int windowHeight = 2;
static int viewportWidth = 2;
static int viewportHeight = 2;
static bool needsResize = false;
static float clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f };

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
