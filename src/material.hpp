#pragma once

#include <memory>
#include <string>

#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <wrl.h>

struct Texture;

using namespace Microsoft::WRL;

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
	bool needsPreviewUpdate = true;
	struct Preview
	{
		ComPtr<ID3D11ShaderResourceView> srv_preview = nullptr;
		ComPtr<ID3D11Texture2D> t_preview = nullptr;
		ComPtr<ID3D11RenderTargetView> rtv_preview = nullptr;
	} preview;

private:
	ID3D11ShaderResourceView* m_srvCache[3] = {nullptr, nullptr, nullptr};
};
