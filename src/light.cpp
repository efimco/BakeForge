#include "light.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

Light::Light(LightType type, glm::vec3 position, std::string _name)
	: type(type)
{
	transform.position = position;
	transform.updateMatrix();
	name = _name;
}

LightData Light::getLightData()
{
	LightData data;
	data.type = type;
	glm::mat4 worldMatrix = getWorldMatrix();
	data.position = glm::vec3(worldMatrix[3]);
	data.intensity = intensity;
	data.color = color;
	data.direction = glm::rotate(direction, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	data.spotParams = spotParams;
	data.attenuations = attenuation;
	data.radius = radius;
	return data;
}

std::unique_ptr<SceneNode> Light::clone()
{
	auto newLight = std::make_unique<Light>(type, transform.position, name);
	newLight->transform = this->transform;
	newLight->visible = this->visible;
	newLight->dirty = this->dirty;
	newLight->movable = this->movable;
	newLight->intensity = this->intensity;
	newLight->color = this->color;
	newLight->direction = this->direction;
	newLight->attenuation = this->attenuation;
	newLight->spotParams = this->spotParams;
	newLight->radius = this->radius;
	return newLight;
}
