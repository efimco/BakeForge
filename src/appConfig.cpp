#include "appConfig.hpp"

static int windowWidth;
static int windowHeight;
static bool needsResize = false;
static float clearColor[4] = {0.4f, 0.6f, 0.9f, 1.0f};

bool &AppConfig::getNeedsResize()
{
	return needsResize;
}

void AppConfig::setNeedsResize(bool _needsResize)
{
	needsResize = _needsResize;
}

int &AppConfig::getWindowWidth()
{
	return windowWidth;
}

int &AppConfig::getWindowHeight()
{
	return windowHeight;
}

void AppConfig::setWindowHeight(int height)
{
	windowHeight = height;
}

void AppConfig::setWindowWidth(int width)
{
	windowWidth = width;
}

float *AppConfig::getClearColor()
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
