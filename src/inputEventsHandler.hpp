#pragma once
enum MouseButtons
{
	LEFT_BUTTON,
	MIDDLE_BUTTON,
	RIGHT_BUTTON
};

enum class KeyButtons
{
	KEY_W,
	KEY_A,
	KEY_S,
	KEY_D,
	KEY_Q,
	KEY_E,
	KEY_F,
	KEY_LSHIFT,
	KEY_LCTRL,
	KEY_UNKNOWNs,
	KEY_F12,
	KEY_F11
};

namespace InputEvents
{
	bool getMouseInViewport();
	void setMouseInViewport(bool state);
	bool isMouseClicked(MouseButtons button);
	bool isMouseDown(MouseButtons button);
	void setMouseDown(MouseButtons button, bool state);
	void setMouseClicked(MouseButtons button, bool state);
	bool isKeyDown(KeyButtons key);
	void setKeyDown(KeyButtons key, bool state);
	void setMouseDelta(float deltaX, float deltaY);
	void getMouseDelta(float& deltaX, float& deltaY);
	float getMouseWheel();
	float setMouseWheel(float wheel);
} // namespace InputEvents
