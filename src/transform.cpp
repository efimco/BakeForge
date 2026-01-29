#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include "transform.hpp"
#include <glm/gtx/matrix_decompose.hpp>

void Transform::updateMatrix()
{

	prevMatrix = matrix;

	// Create transformation matrix: Scale -> Rotate -> Translate
	const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
	const glm::mat4 rotationMatrix =
		glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0));
	const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

	matrix = translationMatrix * rotationMatrix * scaleMatrix;
}

void Transform::translate(const glm::vec3& translation)
{
	position += translation;
	updateMatrix();
}

void Transform::rotate(const glm::vec3& eulerAngles)
{
	rotation += eulerAngles;
	updateMatrix();
}

void Transform::setScale(const glm::vec3& newScale)
{
	scale = newScale;
	updateMatrix();
}

bool Transform::exactlyEqual(const Transform& other) const
{
	constexpr float epsilon = 1e-5f;
	return glm::all(glm::epsilonEqual(position, other.position, epsilon))
		&& glm::all(glm::epsilonEqual(rotation, other.rotation, epsilon))
		&& glm::all(glm::epsilonEqual(scale, other.scale, epsilon));
}

bool Transform::nearlyEqual(const Transform& other) const
{
	return position == other.position
		&& rotation == other.rotation
		&& scale == other.scale;
}
