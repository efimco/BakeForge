#pragma once

#include <memory>
#include <string>

#include <d3d11_4.h>
#include <glm/glm.hpp>

struct Texture;

struct Material
{
	std::string name;
	std::shared_ptr<Texture> albedo;
	glm::vec4 albedoColor; // with alpha channel
	std::shared_ptr<Texture> metallicRoughness;
	float metallicValue;
	float roughnessValue;
	std::shared_ptr<Texture> normal;
	ID3D11ShaderResourceView* const* getSRVs();

private:
	ID3D11ShaderResourceView* m_srvCache[3] = {nullptr, nullptr, nullptr};
};
