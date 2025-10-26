#pragma once
#include "sceneNode.hpp"
#include <glm/glm.hpp>


enum LightType
{
	POINT_LIGHT = 0,
	DIRECTIONAL_LIGHT = 1,
	SPOT_LIGHT = 2
};

struct alignas(16) LightData
{
	LightType type;
	glm::vec3 position;
	float intensity;
	glm::vec3 color;
	glm::vec3 direction;
	float padding1; // Padding to align to 16 bytes
	glm::vec2 spotParams; // x: inner cone angle, y: outer cone angle
	glm::vec3 attenuations;
	float padding2; // Padding to align to 16 bytes
};

class Light : public SceneNode
{
public:
	Light(LightType type, glm::vec3 position, std::string name = "Light");
	LightType getType() const;

private:
	float m_intensity = 1.0f;
	glm::vec3 m_color = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 m_direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 m_attenuation = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec2 m_spotParams = glm::vec2(0.0f, 0.0f); // x: inner cone angle, y: outer cone angle
	LightType m_type = POINT_LIGHT;

	LightData getLightData() const;
};