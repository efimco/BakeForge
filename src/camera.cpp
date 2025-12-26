#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "inputEventsHandler.hpp"
#include "appConfig.hpp"

static const float YAW = 0.0f;
static const float PITCH = 0.0f;
static const float FOV = 45.f;
static const glm::vec3 WORLD_UP = glm::vec3(0, 1, 0);

Camera::Camera(glm::vec3 pos)
	: front(glm::vec3(0.0f, 0.0f, -1.0f)), fov(FOV),
	cameraReseted(true)
{
	// Initialize transform using SceneNode's properties
	transform.position = pos;
	transform.rotation = glm::vec3(PITCH, YAW, 0.0f); // pitch (X), yaw (Y)
	transform.updateMatrix();

	orbitPivot = glm::vec3(0.0f); // point to orbit around
	distanceToOrbitPivot = glm::length(transform.position - orbitPivot);
	updateCameraVectors();
}

void Camera::processMovementControls()
{
	
	if (!InputEvents::getMouseInViewport())
		return;
	processZoom();
	if (InputEvents::isMouseDown(MouseButtons::MIDDLE_BUTTON) && InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		processPanning();
	}
	if (InputEvents::isMouseDown(MouseButtons::MIDDLE_BUTTON) && !InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		processOrbit();
	}
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix()
{
	return glm::lookAtLH(transform.position, orbitPivot, WORLD_UP);
}

void Camera::processZoom()
{
	// Blender-style zoom: more responsive and distance-independent
	float zoomSpeed = 0.1f;
	float zoomFactor = 1.0f + (InputEvents::getMouseWheel() * zoomSpeed);

	// Clamp to prevent getting too close or too far
	float newDistance = glm::clamp(distanceToOrbitPivot / zoomFactor, 0.01f, 1000.0f);
	distanceToOrbitPivot = newDistance;
}

void Camera::processPanning()
{
	float deltaX = 0;
	float deltaY = 0;
	InputEvents::getMouseDelta(deltaX, deltaY);

	float panSpeed = 0.001f; // Adjust this factor for desired sensitivity
	glm::vec3 rightMove = -right * deltaX * distanceToOrbitPivot * panSpeed;
	glm::vec3 upMove = up * deltaY * distanceToOrbitPivot * panSpeed;

	orbitPivot += rightMove + upMove;
}

void Camera::updateCameraVectors()
{
	// Use SceneNode's transform.rotation (degrees) as the source of yaw/pitch
	float yawDeg = transform.rotation.y;
	float pitchDeg = transform.rotation.x;

	glm::vec3 offset;
	offset.x = distanceToOrbitPivot * cos(glm::radians(pitchDeg)) * sin(glm::radians(yawDeg));
	offset.y = distanceToOrbitPivot * sin(glm::radians(pitchDeg));
	offset.z = distanceToOrbitPivot * cos(glm::radians(pitchDeg)) * cos(glm::radians(yawDeg));

	transform.position = orbitPivot + offset;
	transform.updateMatrix();

	// Derive camera basis from world axes and target
	front = glm::normalize(orbitPivot - transform.position);
	right = glm::normalize(glm::cross(WORLD_UP, front));
	up = glm::normalize(glm::cross(front, right));
}

void Camera::processOrbit()
{
	float deltaX = 0;
	float deltaY = 0;
	InputEvents::getMouseDelta(deltaX, deltaY);
	// Blender-style orbit: consistent angular speed
	float orbitSensitivity = 0.5f; // Fixed sensitivity like Blender

	// Apply deltas to SceneNode's transform.rotation (degrees)
	transform.rotation.y += deltaX * orbitSensitivity; // yaw
	transform.rotation.x += deltaY * orbitSensitivity; // pitch

	// Clamp pitch to avoid gimbal lock
	if (transform.rotation.x > 89.0f)
		transform.rotation.x = 89.0f;
	if (transform.rotation.x < -89.0f)
		transform.rotation.x = -89.0f;
}

void Camera::onCommitTransaction(Scene& scene)
{
	updateCameraVectors();
}

void Camera::copyFrom(const SceneNode& node)
{
	SceneNode::copyFrom(node);
	if (const auto cameraNode = dynamic_cast<const Camera*>(&node))
	{
		fov = cameraNode->fov;
	}
}

bool Camera::differsFrom(const SceneNode& node) const
{
	if (!SceneNode::differsFrom(node))
	{
		if (const auto cameraNode = dynamic_cast<const Camera*>(&node))
		{
			return fov != cameraNode->fov;
		}
	}
	return true;
}

std::unique_ptr<SceneNode> Camera::clone() const
{
	std::unique_ptr cameraNode = std::make_unique<Camera>(transform.position);
	cameraNode->copyFrom(*this);
	return cameraNode;
}

// void Camera::focusOn(SceneNode* node)
// {
// 	if (dynamic_cast<Primitive*>(node) == nullptr)
// 		return;
// 	glm::vec3 minPos = primitive->boundingBox.first + glm::vec3(primitive->transform.position);
// 	glm::vec3 maxPos = primitive->boundingBox.second + glm::vec3(primitive->transform.position);
// 	glm::vec3 center = (minPos + maxPos) * 0.5f;
// 	float radius = glm::length(maxPos - minPos) * 0.5f;
// 	orbitPivot = center;

// 	// Position camera to see the entire bounding box (Blender-style framing)
// 	float distance = radius * 2.5f; // More breathing room like Blender
// 	distanceToOrbitPivot = distance;

// 	updateCameraVectors();
// }

