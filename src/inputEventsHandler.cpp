#include "inputEventsHandler.hpp"

#include <iostream>

static bool isMouseInViewport = false;
static bool isLMouseClicked = false;
static bool isMMouseClicked = false;
static bool isRMouseClicked = false;

static bool isLMouseDown = false;
static bool isMMouseDown = false;
static bool isRMouseDown = false;

static float mouseDeltaX = 0.0f;
static float mouseDeltaY = 0.0f;
static float mouseWheel = 0;

static bool isKeyWPressed = false;
static bool isKeyAPressed = false;
static bool isKeySPressed = false;
static bool isKeyDPressed = false;
static bool isKeyQPressed = false;
static bool isKeyEPressed = false;
static bool isKeyLShiftPressed = false;

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

bool InputEvents::isMouseDown(MouseButtons button)
{
	switch (button)
	{
	case LEFT_BUTTON:
		return isLMouseDown;
		break;
	case MIDDLE_BUTTON:
		return isMMouseDown;
		break;
	case RIGHT_BUTTON:
		return isRMouseDown;
		break;
	default:
		return false;
		break;
	}
}

void InputEvents::setMouseDown(MouseButtons button, bool state)
{
	switch (button)
	{
	case LEFT_BUTTON:
		isLMouseDown = state;
		break;
	case MIDDLE_BUTTON:
		isMMouseDown = state;
		break;
	case RIGHT_BUTTON:
		isRMouseDown = state;
		break;
	default:
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

bool InputEvents::isKeyDown(KeyButtons key)
{
	switch (key)
	{
	case KeyButtons::KEY_W:
		return isKeyWPressed;
		break;
	case KeyButtons::KEY_A:
		return isKeyAPressed;
		break;
	case KeyButtons::KEY_S:
		return isKeySPressed;
		break;
	case KeyButtons::KEY_D:
		return isKeyDPressed;
		break;
	case KeyButtons::KEY_Q:
		return isKeyQPressed;
		break;
	case KeyButtons::KEY_E:
		return isKeyEPressed;
		break;
	case KeyButtons::KEY_LSHIFT:
		return isKeyLShiftPressed;
		break;
	default:
		return false;
		break;
	}
}

void InputEvents::setKeyDown(KeyButtons key, bool state)
{
	switch (key)
	{
	case KeyButtons::KEY_W:
		isKeyWPressed = state;
		break;
	case KeyButtons::KEY_A:
		isKeyAPressed = state;
		break;
	case KeyButtons::KEY_S:
		isKeySPressed = state;
		break;
	case KeyButtons::KEY_D:
		isKeyDPressed = state;
		break;
	case KeyButtons::KEY_Q:
		isKeyQPressed = state;
		break;
	case KeyButtons::KEY_E:
		isKeyEPressed = state;
		break;
	case KeyButtons::KEY_LSHIFT:
		isKeyLShiftPressed = state;
		break;
	default:
		break;
	}
}

void InputEvents::setMouseDelta(float deltaX, float deltaY)
{
	mouseDeltaX = deltaX;
	mouseDeltaY = deltaY;
}

void InputEvents::getMouseDelta(float& deltaX, float& deltaY)
{
	deltaX = mouseDeltaX;
	deltaY = mouseDeltaY;
}

float InputEvents::getMouseWheel()
{
	return mouseWheel;
}

float InputEvents::setMouseWheel(float wheel)
{
	mouseWheel = wheel;
	return mouseWheel;
}


