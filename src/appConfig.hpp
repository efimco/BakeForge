namespace AppConfig
{
	bool& getNeedsResize();
	void setNeedsResize(bool _needsResize);

	const int& getWindowWidth();
	const int& getWindowHeight();
	const int& getViewportWidth();
	const int& getViewportHeight();
	void setViewportWidth(int width);
	void setViewportHeight(int height);
	void setWindowWidth(int width);
	void setWindowHeight(int height);

	float* getClearColor();
	void setClearColor(float r, float g, float b, float a);

}