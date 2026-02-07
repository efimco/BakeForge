#pragma once


#include <d3d11_4.h>
#include <filesystem>
#include <future>
#include <string>
#include <wrl.h>

#include "tiny_gltf.h"

using namespace Microsoft::WRL;

struct Texture;

struct TextureLoadProgress
{
	std::atomic<bool> isCompleted = false;
	std::atomic<bool> hasFailed = false;
	std::string errorMessage;
};


struct AsyncTextureResult
{
	std::shared_ptr<Texture> texture;
	ComPtr<ID3D11CommandList> commandList;
	std::shared_ptr<TextureLoadProgress> progress;
};

struct PendingTextureReload
{
	std::string name;
	std::future<AsyncTextureResult> future;
	std::shared_ptr<TextureLoadProgress> progress;
};


struct Texture
{
	std::string name;
	std::string filepath;
	std::filesystem::file_time_type lastModifiedTime;
	ComPtr<ID3D11Texture2D> textureResource;
	ComPtr<ID3D11ShaderResourceView> srv;
	D3D11_TEXTURE2D_DESC texDesc;

	explicit Texture(const ComPtr<ID3D11Device>& device);
	Texture(Texture&& other, ComPtr<ID3D11Device> device);
	Texture(const Texture& other) = delete;
	Texture(const tinygltf::Image& image, ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context = nullptr);
	Texture(std::string filepath, ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context = nullptr);

	uint32_t getWidth() const;
	uint32_t getHeight() const;

	static std::future<AsyncTextureResult> loadTextureAsync(const std::string& filepath,
		ComPtr<ID3D11Device> device,
		std::shared_ptr<TextureLoadProgress> progress = nullptr);

	static void finalizeAsyncLoad(AsyncTextureResult result,
		ComPtr<ID3D11DeviceContext> immContext);

	ComPtr<ID3D11Device> device;
};
