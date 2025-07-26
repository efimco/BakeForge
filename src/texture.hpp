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
	Texture() = default;
	Texture(const char* path);
	Texture(tinygltf::Image& image);
};

Texture::Texture(const char* path)
{
	
}