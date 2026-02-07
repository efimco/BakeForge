#include "texture.hpp"

#include <iostream>

#include "DirectXTex.h"

Texture::Texture(const ComPtr<ID3D11Device>& _device)
	: device(_device)
{
}

Texture::Texture(Texture&& other, ComPtr<ID3D11Device> _device)
{
	device = _device;
	texDesc = std::move(other.texDesc);
	name = std::move(other.name);
	filepath = std::move(other.filepath);
	textureResource = std::move(other.textureResource);
}


Texture::Texture(const tinygltf::Image& image, ComPtr<ID3D11Device> _device, ComPtr<ID3D11DeviceContext> _context)
{
	device = _device;
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
	texDesc.MipLevels = 0; // Let D3D calculate mip count
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	// When MipLevels = 0, cannot provide initial data - must pass nullptr
	{
		HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &textureResource);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create texture resource = " << hr << std::endl;
		}
	}

	// Get actual mip count after creation
	textureResource->GetDesc(&texDesc);

	// Upload base mip level data
	ComPtr<ID3D11DeviceContext> context = _context;
	if (!context)
	{
		device->GetImmediateContext(&context);
	}
	context->UpdateSubresource(textureResource.Get(), 0, nullptr, image.image.data(),
		image.width * image.component, 0);

	// Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1; // Use all mip levels
	{
		HRESULT hr = device->CreateShaderResourceView(textureResource.Get(), &srvDesc, &srv);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create shader resource view = " << hr << std::endl;
		}
	}
	context->GenerateMips(srv.Get());
}

uint32_t GetBytesPerPixel(DXGI_FORMAT format);

Texture::Texture(std::string filepath, ComPtr<ID3D11Device> _device, ComPtr<ID3D11DeviceContext> context)
{

	device = _device;

	bool isTga = false;
	bool isDds = false;
	bool isWic = false;
	bool isHDR = false;

	if (filepath.ends_with(".tga") || filepath.ends_with(".TGA"))
	{
		isTga = true;
	}
	else if (filepath.ends_with(".dds") || filepath.ends_with(".DDS"))
	{
		isDds = true;
	}
	else if (filepath.ends_with(".hdr") || filepath.ends_with(".HDR"))
	{
		isHDR = true;
	}
	else
	{
		isWic = true;
	}

	DirectX::ScratchImage tempImage;
	HRESULT hr = S_OK;
	if (isTga)
	{
		hr = DirectX::LoadFromTGAFile(
			std::wstring(filepath.begin(), filepath.end()).c_str(),
			DirectX::TGA_FLAGS_NONE,
			nullptr,
			tempImage);
	}
	else if (isDds)
	{
		hr = DirectX::LoadFromDDSFile(
			std::wstring(filepath.begin(), filepath.end()).c_str(),
			DirectX::DDS_FLAGS_NONE,
			nullptr,
			tempImage);
	}
	else if (isHDR)
	{
		hr = DirectX::LoadFromHDRFile(
			std::wstring(filepath.begin(), filepath.end()).c_str(),
			nullptr,
			tempImage);

		if (SUCCEEDED(hr) && tempImage.GetMetadata().format == DXGI_FORMAT_R32G32B32_FLOAT)
		{
			DirectX::ScratchImage converted;
			hr = DirectX::Convert(
				tempImage.GetImages(),
				tempImage.GetImageCount(),
				tempImage.GetMetadata(),
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				converted);
			if (SUCCEEDED(hr))
			{
				tempImage = std::move(converted);
			}
		}
	}
	else if (isWic)
	{
		hr = DirectX::LoadFromWICFile(
			std::wstring(filepath.begin(), filepath.end()).c_str(),
			DirectX::WIC_FLAGS_NONE,
			nullptr,
			tempImage);
	}

	DirectX::ScratchImage mipChain;
	hr = DirectX::GenerateMipMaps(tempImage.GetImages(), tempImage.GetImageCount(), tempImage.GetMetadata(), DirectX::TEX_FILTER_DITHER, 0, mipChain);
	if (SUCCEEDED(hr))
	{
		tempImage = std::move(mipChain);
	}

	lastModifiedTime = std::filesystem::last_write_time(filepath);
	name = filepath.find_last_of("/\\") != std::string::npos ?
		filepath.substr(filepath.find_last_of("/\\") + 1) : filepath;


	this->filepath = filepath;

	const auto& metadata = tempImage.GetMetadata();
	const auto* image = tempImage.GetImage(0, 0, 0);

	// Use DirectXTex helper to create texture - handles row pitch correctly
	ComPtr<ID3D11Resource> resource;
	hr = DirectX::CreateTexture(
		device.Get(),
		tempImage.GetImages(),
		tempImage.GetImageCount(),
		metadata,
		&resource);

	if (FAILED(hr))
	{
		std::cerr << "Failed to create texture " << filepath << " hr=" << hr << std::endl;
		return;
	}

	hr = resource.As(&textureResource);
	if (FAILED(hr))
	{
		std::cerr << "Failed to cast resource to Texture2D" << std::endl;
		return;
	}

	textureResource->GetDesc(&texDesc);

	// Create SRV
	hr = DirectX::CreateShaderResourceView(
		device.Get(),
		tempImage.GetImages(),
		tempImage.GetImageCount(),
		metadata,
		&srv);

	if (FAILED(hr))
	{
		std::cerr << "Failed to create shader resource view = " << hr << std::endl;
		return;
	}
}

uint32_t Texture::getWidth() const
{
	return texDesc.Width;
}

uint32_t Texture::getHeight() const
{
	return texDesc.Height;
}

std::future<AsyncTextureResult> Texture::loadTextureAsync(const std::string& filepath, ComPtr<ID3D11Device> device, std::shared_ptr<TextureLoadProgress> progress)
{
	if (!progress)
	{
		progress = std::make_shared<TextureLoadProgress>();
	}
	return std::async(std::launch::async, [filepath, device, progress]() -> AsyncTextureResult
		{
			AsyncTextureResult result;
			result.progress = progress;
			try
			{
				ComPtr<ID3D11DeviceContext> deferredContext;
				HRESULT hr = device->CreateDeferredContext(0, &deferredContext);
				if (FAILED(hr))
				{
					progress->errorMessage = "Failed to create deferred context";
					progress->hasFailed = true;
					progress->isCompleted = true;
					return result;
				}

				result.texture = std::make_shared<Texture>(filepath, device, deferredContext);

				if (!result.texture->textureResource)
				{
					progress->errorMessage = "Failed to create texture: " + filepath;
					progress->hasFailed = true;
					progress->isCompleted = true;
					return result;
				}

				hr = deferredContext->FinishCommandList(FALSE, &result.commandList);
				if (FAILED(hr))
				{
					progress->errorMessage = "Failed to finish command list";
					progress->hasFailed = true;
					progress->isCompleted = true;
					return result;
				}
				progress->isCompleted = true;
			}
			catch (const std::exception& e)
			{
				progress->errorMessage = e.what();
				progress->hasFailed = true;
				progress->isCompleted = true;
			}

			return result;
		});
}

void Texture::finalizeAsyncLoad(AsyncTextureResult result, ComPtr<ID3D11DeviceContext> immContext)
{
	if (result.progress && result.progress->hasFailed)
	{
		std::cerr << "Async texture load failed: " << result.progress->errorMessage << std::endl;
		return;
	}
	if (result.commandList)
	{
		immContext->ExecuteCommandList(result.commandList.Get(), FALSE);
	}
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
	default:
		return 0;
	}
}
