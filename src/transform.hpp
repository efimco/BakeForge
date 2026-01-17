#pragma once

#include <glm/glm.hpp>

struct Transform
{

	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	glm::mat4 matrix = glm::mat4(1.0f);
	glm::mat4 prevMatrix = glm::mat4(1.0f);

	void updateMatrix();

	void translate(const glm::vec3& translation);
	void rotate(const glm::vec3& rotation);
	void setScale(const glm::vec3& scale);

	bool exactlyEqual(const Transform& other) const;
	bool nearlyEqual(const Transform& other) const;
};
