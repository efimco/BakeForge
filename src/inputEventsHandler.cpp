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

bool InputEvents::isMouseClicked(MouseButtons button)
{
	switch (button)
	{
	case LEFT_BUTTON:
		return isLMouseClicked;
		break;
	case MIDDLE_BUTTON:
		return isMMouseClicked;
		break;
	case RIGHT_BUTTON:
		return isRMouseClicked;
		break;
	default:
		return false;
		break;
	}
}

void InputEvents::setMouseClicked(MouseButtons button, bool state)
{
	switch (button)
	{
	case LEFT_BUTTON:
		if (!isLMouseClicked && state)
		{
			std::cout << "Left mouse button clicked in viewport." << std::endl;
		}
		isLMouseClicked = state;
		break;
	case MIDDLE_BUTTON:
		if (!isMMouseClicked && state)
		{
			std::cout << "Middle mouse button clicked in viewport." << std::endl;
		}
		isMMouseClicked = state;
		break;
	case RIGHT_BUTTON:
		if (!isRMouseClicked && state)
		{
			std::cout << "Right mouse button clicked in viewport." << std::endl;
		}
		isRMouseClicked = state;
		break;
	default:
		break;
	}
}


