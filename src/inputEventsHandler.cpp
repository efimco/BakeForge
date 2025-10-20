#include "inputEventsHandler.hpp"
#include <iostream>
static bool isMouseInViewport = false;
static bool isLMouseClicked = false;
static bool isMMouseClicked = false;
static bool isRMouseClicked = false;

bool InputEvents::getMouseInViewport()
{
	return isMouseInViewport;
}
void InputEvents::setMouseInViewport(bool state)
{
	isMouseInViewport = state;
}

void InputEvents::setLMouseClicked(bool state)
{
	if (!isLMouseClicked && state)
	{
		std::cout << "Left mouse button clicked in viewport." << std::endl;
	}
	isLMouseClicked = state;
}

bool InputEvents::getLMouseClicked()
{
	return isLMouseClicked;
}

void InputEvents::setMMouseClicked(bool state)
{
	if (!isMMouseClicked && state)
	{
		std::cout << "Middle mouse button clicked in viewport." << std::endl;
	}
	isMMouseClicked = state;
}

bool InputEvents::getMMouseClicked()
{
	return isMMouseClicked;
}

void InputEvents::setRMouseClicked(bool state)
{
	if (!isRMouseClicked && state)
	{
		std::cout << "Right mouse button clicked in viewport." << std::endl;
	}
	isRMouseClicked = state;
}

bool InputEvents::getRMouseClicked()
{
	return isRMouseClicked;
}
