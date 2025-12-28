#include "texture.hpp"

#include <iostream>

#include "stb_image.h"

Texture::Texture(const ComPtr<ID3D11Device>& _device) : device(_device) {}

Texture::Texture(Texture&& other, const ComPtr<ID3D11Device>& _device) : device(_device)
{
	texDesc = std::move(other.texDesc);
	name = std::move(other.name);
	filepath = std::move(other.filepath);
	textureResource = std::move(other.textureResource);
}


Texture::Texture(const tinygltf::Image& image, const ComPtr<ID3D11Device>& _device) : device(_device)
{
	name = image.name;
	filepath = image.uri;
	texDesc.Height = image.height;
	texDesc.Width = image.width;
	texDesc.CPUAccessFlags = 0;
	DXGI_FORMAT format;
	if (image.component == 1)
		format = DXGI_FORMAT_R8_UNORM;
	else if (image.component == 2)
		format = DXGI_FORMAT_R8G8_UNORM;
	else
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Format = format;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	D3D11_SUBRESOURCE_DATA subresData;
	subresData.pSysMem = image.image.data();
	subresData.SysMemPitch = image.width * image.component;
	subresData.SysMemSlicePitch = 0;


	{
		HRESULT hr = device->CreateTexture2D(&texDesc, &subresData, &textureResource);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create texture resource = " << hr << std::endl;
		}
	}
	// Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	{
		HRESULT hr = device->CreateShaderResourceView(textureResource.Get(), &srvDesc, &srv);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create shader resource view = " << hr << std::endl;
		}
	}
	ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(&context);
	context->GenerateMips(srv.Get());
}
uint32_t GetBytesPerPixel(DXGI_FORMAT format);

Texture::Texture(std::string filepath, const ComPtr<ID3D11Device>& _device, bool isHdr) : device(_device)
{
	int width, height, channels;
	void* data = nullptr;
	if (isHdr)
	{
		data = stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	}
	else
	{
		data = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	}
	if (!data)
	{
		std::cerr << "Failed to load texture image: " << filepath << std::endl;
		return;
	}
	name = filepath;
	this->filepath = filepath;
	texDesc.Height = height;
	texDesc.Width = width;
	texDesc.CPUAccessFlags = 0;
	DXGI_FORMAT format = isHdr ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Format = format;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.MiscFlags = isHdr ? 0 : D3D11_RESOURCE_MISC_GENERATE_MIPS;

	D3D11_SUBRESOURCE_DATA subresData;
	subresData.pSysMem = data;
	subresData.SysMemPitch = width * GetBytesPerPixel(format);
	subresData.SysMemSlicePitch = 0;

	{
		HRESULT hr = device->CreateTexture2D(&texDesc, &subresData, &textureResource);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create texture resource = " << hr << std::endl;
		}
	}
	// Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	{
		HRESULT hr = device->CreateShaderResourceView(textureResource.Get(), &srvDesc, &srv);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create shader resource view = " << hr << std::endl;
		}
	}
	ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(&context);
	if (!isHdr)
		context->GenerateMips(srv.Get());
	stbi_image_free(data);
}

uint32_t Texture::getWidth() const
{
	return texDesc.Width;
}

uint32_t Texture::getHeight() const
{
	return texDesc.Height;
}

uint32_t GetBytesPerPixel(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return 4;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return 16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return 8;
		// Add more formats as needed
	default:
		return 0; // Unknown format
	}
}
