#include "inputEventsHandler.hpp"
static bool isMouseInViewport = false;
static bool isMouseClicked = false;

bool InputEvents::getMouseInViewport()
{
	return isMouseInViewport;
}
void InputEvents::setMouseInViewport(bool state)
{
	isMouseInViewport = state;
}

void InputEvents::setMouseClicked(bool state)
{
	isMouseClicked = state;
}
