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
	glm::vec2 padding2; // Additional padding to align to 16 bytes
	glm::vec3 attenuations;
	float radius; // radius for point lights
};

class Light : public SceneNode
{
public:
	Light(LightType type, glm::vec3 position, std::string name = "Light");
	LightData getLightData();

	float intensity = 1.0f;
	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 attenuation = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec2 spotParams = glm::vec2(0.0f, 0.0f); // x: inner cone angle, y: outer cone angle
	float radius = 1.0f;
	LightType type = POINT_LIGHT;


};