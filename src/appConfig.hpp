namespace AppConfig
{
	bool& getNeedsResize();
	void setNeedsResize(bool _needsResize);
	int& getWindowWidth();
	int& getWindowHeight();
	void setWindowHeight(int height);
	void setWindowWidth(int width);
	float* getClearColor();
	void setClearColor(float r, float g, float b, float a);
}