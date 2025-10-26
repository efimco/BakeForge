#include "light.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

Light::Light(LightType type, glm::vec3 position, std::string _name)
	: m_type(type)
{
	transform.position = position;
	transform.updateMatrix();
	name = _name;
}

LightType Light::getType() const
{
	return m_type;
}

LightData Light::getLightData() const
{
	LightData data;
	data.type = m_type;
	data.position = transform.position;
	data.intensity = m_intensity;
	data.color = m_color;
	data.direction = glm::rotate(m_direction, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	data.spotParams = m_spotParams;
	data.attenuations = m_attenuation;
	data.padding1 = 0.0f;
	data.padding2 = 0.0f;
	return data;
}
