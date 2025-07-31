#include "appConfig.hpp"


static int windowWidth;
static int windowHeight;

int& AppConfig::getWindowWidth()
{
	return windowWidth;
}

int& AppConfig::getWindowHeight()
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
