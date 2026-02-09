#pragma once

#include <memory>
#include <string_view>

#include <d3d11_4.h>
#include <string>
#include <glm/glm.hpp>
#include <wrl.h>

#include "utility/stringUnorderedMap.hpp"
#include "basePass.hpp"

using namespace Microsoft::WRL;

struct TextureSnapshot
{
	ComPtr<ID3D11Texture2D> m_textureCopy;
	ComPtr<ID3D11ShaderResourceView> m_textureCopySRV;

	ComPtr<ID3D11Buffer> m_tileIndexBuffer;
	ComPtr<ID3D11Buffer> m_tileStagingBuffer;
	ComPtr<ID3D11UnorderedAccessView> m_tileIndexUAV;
};

struct TextureDelta
{
	uint32_t m_width = 0;
	uint32_t m_height = 0;

	ComPtr<ID3D11Texture2D> m_textureTiles;
	std::vector<uint16_t> m_tileIndices;
};

struct alignas(16) TextureHistoryCB
{
	uint32_t height;
	uint32_t width;
	uint32_t tileSize;
	uint32_t tileNumX;
	uint32_t tileNumY;
	uint32_t numTiles;
};

class TextureHistory : public BasePass
{
public:
	TextureHistory(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~TextureHistory() override = default;

	bool HasSnapshot(std::string_view name) const;

	std::shared_ptr<TextureSnapshot> StartSnapshot(
		std::string_view name,
		ComPtr<ID3D11Texture2D> texture,
		bool forceFresh = false);

	void EndSnapshot(
		std::string_view name);

	std::shared_ptr<TextureDelta> CreateDelta(
		std::string_view name,
		ComPtr<ID3D11Texture2D> texture,
		ComPtr<ID3D11ShaderResourceView> textureSRV);

	void ApplyDelta(
		ComPtr<ID3D11Texture2D> texture,
		std::shared_ptr<TextureDelta> textureDelta);

private:
	void updateConstantBuffer(
		TextureHistoryCB& cb);

	//  ## Resources for paint history ##

	// Size of a history tile - cannot be smaller than 8
	static constexpr uint32_t k_textureHistoryTileSize     = 64;
	static constexpr uint32_t k_textureHistoryTileSizeMOne = k_textureHistoryTileSize - 1;

	ComPtr<ID3D11Buffer> m_constantBuffer;
	StringUnorderedMap<std::shared_ptr<TextureSnapshot>> m_snapshots;
};
