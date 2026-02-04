#include "inputEventsHandler.hpp"

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
static bool isKeyFPressed = false;
static bool isKeyLShiftPressed = false;
static bool isKeyLCtrlPressed = false;
static bool isKeyF11Pressed = false;
static bool isKeyF12Pressed = false;

bool InputEvents::getMouseInViewport()
{
	return isMouseInViewport;
}

void InputEvents::setMouseInViewport(const bool state)
{
	isMouseInViewport = state;
}

bool InputEvents::isMouseClicked(const MouseButtons button)
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

bool InputEvents::isMouseDown(const MouseButtons button)
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

void InputEvents::setMouseDown(const MouseButtons button, const bool state)
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

void InputEvents::setMouseClicked(const MouseButtons button, const bool state)
{
	switch (button)
	{
	case LEFT_BUTTON:
		isLMouseClicked = state;
		break;
	case MIDDLE_BUTTON:
		isMMouseClicked = state;
		break;
	case RIGHT_BUTTON:
		isRMouseClicked = state;
		break;
	default:
		break;
	}
}

bool InputEvents::isKeyDown(const KeyButtons key)
{
	switch (key)
	{
	case KeyButtons::KEY_W:
		return isKeyWPressed;
	case KeyButtons::KEY_A:
		return isKeyAPressed;
	case KeyButtons::KEY_S:
		return isKeySPressed;
	case KeyButtons::KEY_D:
		return isKeyDPressed;
	case KeyButtons::KEY_Q:
		return isKeyQPressed;
	case KeyButtons::KEY_E:
		return isKeyEPressed;
	case KeyButtons::KEY_F:
		return isKeyFPressed;
	case KeyButtons::KEY_LSHIFT:
		return isKeyLShiftPressed;
	case KeyButtons::KEY_LCTRL:
		return isKeyLCtrlPressed;
	case KeyButtons::KEY_F11:
		return isKeyF11Pressed;
	case KeyButtons::KEY_F12:
		return isKeyF12Pressed;
	default:
		return false;
	}
}

void InputEvents::setKeyDown(const KeyButtons key, const bool state)
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
	case KeyButtons::KEY_F:
		isKeyFPressed = state;
		break;
	case KeyButtons::KEY_LSHIFT:
		isKeyLShiftPressed = state;
		break;
	case KeyButtons::KEY_LCTRL:
		isKeyLCtrlPressed = state;
		break;
	case KeyButtons::KEY_F11:
		isKeyF11Pressed = state;
		break;
	default:
		break;
	}
}

void InputEvents::setMouseDelta(const float deltaX, const float deltaY)
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

float InputEvents::setMouseWheel(const float wheel)
{
	mouseWheel = wheel;
	return mouseWheel;
}
