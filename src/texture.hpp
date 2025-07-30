#pragma once

#include <string>
#include <tiny_gltf.h>
#include <wrl.h>
#include <d3d11.h>

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
	const ComPtr<ID3D11Device>& device;
};
