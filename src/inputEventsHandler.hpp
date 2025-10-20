#pragma once
enum MouseButtons
{
	LEFT_BUTTON,
	MIDDLE_BUTTON,
	RIGHT_BUTTON
};

namespace InputEvents
{
	bool getMouseInViewport();
	void setMouseInViewport(bool state);
	bool isMouseClicked(MouseButtons button);
	void setMouseClicked(MouseButtons button, bool state);
}