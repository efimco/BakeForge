#pragma once

#include <glm/glm.hpp>

#include "sceneNode.hpp"

class Camera : public SceneNode
{
public:
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;
	glm::vec3 orbitPivot;

	float fov = 45.0f;
	float distanceToOrbitPivot = 0.f;

	explicit Camera(glm::vec3 pos, std::string_view nodeName = "Camera");
	void processMovementControls();
	glm::mat4 getViewMatrix();

	void updateCameraVectors();

private:
	void processZoom();
	void processPanning();
	void processOrbit();

	// void focusOn(Primitive* primitive);

	virtual void onCommitTransaction(Scene& scene) override;
	virtual void copyFrom(const SceneNode& node) override;
	virtual bool differsFrom(const SceneNode& node) const override;
	virtual std::unique_ptr<SceneNode> clone() const override;
};
