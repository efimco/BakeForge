#pragma once
#include "texture.hpp"
#include <glm/glm.hpp>
#include <string>
#include <memory>

struct Material
{
	std::string name;
	std::shared_ptr<Texture> albedo;
	glm::vec4 albedoColor; // with alpha channel
	std::shared_ptr<Texture> metallicRoughness;
	float metallicValue;
	float roughnessValue;
	std::shared_ptr<Texture> normal;
};