#pragma once

#include <string>

#include <wrl.h>
#include <d3d11.h>

#include <tiny_gltf.h>

using namespace Microsoft::WRL;

struct Texture
{
	std::string name;
	std::string filepath;
	ComPtr<ID3D11Texture2D> textureResource;
	ComPtr<ID3D11ShaderResourceView> srv;
	D3D11_TEXTURE2D_DESC texDesc;
	Texture(const ComPtr<ID3D11Device>& device);
	Texture(Texture&& other, const ComPtr<ID3D11Device>& device);
	Texture(const Texture& other) = delete;
	Texture(const tinygltf::Image& image, const ComPtr<ID3D11Device>& device);
	Texture(std::string filepath, const ComPtr<ID3D11Device>& device, bool isHdr = false);
	uint32_t getWidth() const;
	uint32_t getHeight() const;
	const ComPtr<ID3D11Device>& device;
};
