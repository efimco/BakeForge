#pragma once
#include "glm/glm.hpp"
#include "sceneNode.hpp"
#include <vector>


class Camera : public SceneNode
{
public:
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;
	glm::vec3 orbitPivot;
	bool cameraReseted;

	float fov;
	float distanceToOrbitPivot;

	Camera(glm::vec3 pos);
	void processMovementControls();
	glm::mat4 getViewMatrix();

	void updateCameraVectors();

private:
	void processZoom();
	void processPanning();
	void processOrbit();

	// void focusOn(Primitive* primitive);

};