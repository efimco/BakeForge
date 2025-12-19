#pragma once
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <d3d11.h>
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
	ID3D11ShaderResourceView* m_srvCache[3] = { nullptr, nullptr, nullptr };
};