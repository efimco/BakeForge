#pragma once
#include "glm/glm.hpp"
#include "sceneNode.hpp"
#include <vector>


class Camera : public SceneNode
{
public:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;
	glm::vec3 orbitPivot;
	bool cameraReseted;

	float yaw;
	float pitch;
	float zoom;
	float speed;
	float defaultSpeed;
	float increasedSpeed;
	float sensitivity;
	float distanceToOrbitPivot;

	Camera(glm::vec3 pos);
	void processMovementControls();
	glm::mat4 getViewMatrix();

private:
	void processZoom();
	void processPanning();
	void processOrbit();
	// void focusOn(Primitive* primitive);

	void updateCameraVectors();
};