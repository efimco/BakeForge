#include "textureHistory.hpp"

#include "shaderManager.hpp"
#include "renderdoc/renderdoc_app.h"

TextureHistory::TextureHistory(
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_constantBuffer = createConstantBuffer(sizeof(TextureHistoryCB));

	m_shaderManager->LoadComputeShader(
		"textureHistoryDifference",
		ShaderManager::GetShaderPath(L"textureHistoryDifference.hlsl"), "CS");
}

bool TextureHistory::HasSnapshot(std::string_view name) const
{
	return m_snapshots.contains(name);
}

std::shared_ptr<TextureSnapshot> TextureHistory::StartSnapshot(
	std::string_view name,
	ComPtr<ID3D11Texture2D> texture,
	bool forceFresh)
{
	if (forceFresh)
	{
		EndSnapshot(name);
	}

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	auto createNewTexture = [this] (std::string_view name, D3D11_TEXTURE2D_DESC& texDesc)
	{
		std::shared_ptr<TextureSnapshot> result = std::make_shared<TextureSnapshot>();
		result->m_textureCopy = createTexture2D(
			texDesc.Width,
			texDesc.Height,
			texDesc.Format,
			D3D11_BIND_SHADER_RESOURCE);
		result->m_textureCopySRV = createShaderResourceView(result->m_textureCopy.Get(), SRVPreset::Texture2D);

		UINT maxNumTiles = (texDesc.Width * texDesc.Height) / (k_textureHistoryTileSize * k_textureHistoryTileSize);
		if (maxNumTiles > 0)
		{
			result->m_tileIndexBuffer = createStructuredBuffer(sizeof(uint32_t), maxNumTiles, SBPreset::Default);
			result->m_tileIndexUAV = createUnorderedAccessView(result->m_tileIndexBuffer.Get(), UAVPreset::StructuredBuffer, 0, maxNumTiles);
			result->m_tileStagingBuffer = createStructuredBuffer(sizeof(uint32_t), maxNumTiles, SBPreset::CpuRead);
		}
		else
		{
			result->m_tileIndexBuffer = nullptr;
			result->m_tileIndexUAV = nullptr;
			result->m_tileStagingBuffer = nullptr;
		}
		m_snapshots.emplace(name, result);
		return result;
	};

	std::shared_ptr<TextureSnapshot> result;
	auto snapshotIt = m_snapshots.find(name);
	if (snapshotIt == m_snapshots.end())
	{
		result = createNewTexture(name, texDesc);
	}
	else
	{
		result = snapshotIt->second;
		D3D11_TEXTURE2D_DESC snapshotTexDesc;
		result->m_textureCopy->GetDesc(&snapshotTexDesc);
		if (snapshotTexDesc.Width != texDesc.Width || snapshotTexDesc.Height != texDesc.Height)
		{
			result = createNewTexture(name, texDesc);
		}
	}

	m_context->CopyResource(result->m_textureCopy.Get(), texture.Get());
	return result;
}

void TextureHistory::EndSnapshot(std::string_view name)
{
	m_snapshots.erase(name.data());
}

std::shared_ptr<TextureDelta> TextureHistory::CreateDelta(
	std::string_view name,
	ComPtr<ID3D11Texture2D> texture,
	ComPtr<ID3D11ShaderResourceView> textureSRV)
{
	std::shared_ptr<TextureDelta> result = std::make_shared<TextureDelta>();
	auto snapshotIt = m_snapshots.find(name);
	if (snapshotIt == m_snapshots.end())
	{
		return result;
	}
	std::shared_ptr<TextureSnapshot> snapshot = snapshotIt->second;

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	UINT maxNumTiles = (texDesc.Width * texDesc.Height) / (k_textureHistoryTileSize * k_textureHistoryTileSize);

	std::vector<uint32_t> indices;
	indices.resize(maxNumTiles);

	uint16_t tileNumX = (texDesc.Width  + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	uint16_t tileNumY = (texDesc.Height + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	UINT modX = texDesc.Width  % k_textureHistoryTileSize;
	UINT modY = texDesc.Height % k_textureHistoryTileSize;

	TextureHistoryCB textureHistoryCB;
	textureHistoryCB.width    = texDesc.Width;
	textureHistoryCB.height   = texDesc.Height;
	textureHistoryCB.tileSize = k_textureHistoryTileSize;
	textureHistoryCB.tileNumX = tileNumX;
	textureHistoryCB.tileNumY = tileNumY;
	textureHistoryCB.numTiles = maxNumTiles;
	updateConstantBuffer(textureHistoryCB);

	m_context->CSSetShader(m_shaderManager->getComputeShader("textureHistoryDifference"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->CSSetShaderResources(0, 1, textureSRV.GetAddressOf());
	m_context->CSSetShaderResources(1, 1, snapshot->m_textureCopySRV.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, snapshot->m_tileIndexUAV.GetAddressOf(), nullptr);
	m_context->Dispatch(textureHistoryCB.tileNumX, textureHistoryCB.tileNumY, 1);

	unbindShaderResources(0, 1);
	unbindShaderResources(1, 1);
	unbindComputeUAVs(0, 1);

	m_context->CopyResource(snapshot->m_tileStagingBuffer.Get(), snapshot->m_tileIndexBuffer.Get());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	{
		HRESULT hr = m_context->Map(snapshot->m_tileStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		assert(SUCCEEDED(hr));
		memcpy(indices.data(), mapped.pData, maxNumTiles * sizeof(uint32_t));
		m_context->Unmap(snapshot->m_tileStagingBuffer.Get(), 0);
	}

	auto middle = std::partition(indices.begin(), indices.end(), [](uint32_t x)
	{
		return x > 0;
	});
	indices.resize(std::distance(indices.begin(), middle));

	if (indices.empty())
	{
		return result;
	}

	result->m_height = texDesc.Height;
	result->m_width = texDesc.Width;
	result->m_textureTiles = createTexture2D(
		texDesc.Width,
		texDesc.Height,
		DXGI_FORMAT_R32_FLOAT,
		0,
		1,
		(UINT)indices.size());
	result->m_tileIndices.insert(result->m_tileIndices.end(), indices.begin(), indices.end());

	for (UINT i : result->m_tileIndices)
	{
		UINT numX = i % tileNumX;
		UINT numY = i / tileNumX;
		UINT offX = numX * k_textureHistoryTileSize;
		UINT offY = numY * k_textureHistoryTileSize;
		UINT sizeX = numX != tileNumX || modX == 0 ? k_textureHistoryTileSize : modX;
		UINT sizeY = numY != tileNumY || modX == 0 ? k_textureHistoryTileSize : modY;
		D3D11_BOX box {
			offX, offY, 0,
			offX + sizeX, offY + sizeY, 1 };
		m_context->CopySubresourceRegion(
			snapshot->m_textureCopy.Get(),
			i,
			0, 0, 0,
			texture.Get(),
			0,
			&box
		);
	}

	return result;
}

void TextureHistory::ApplyDelta(
	ComPtr<ID3D11Texture2D> texture,
	std::shared_ptr<TextureDelta> textureDelta)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);
	if (texDesc.Width != textureDelta->m_width || texDesc.Height != textureDelta->m_height)
	{
		return;
	}

	uint16_t tileNumX = (texDesc.Width  + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	uint16_t tileNumY = (texDesc.Height + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	UINT modX = texDesc.Width  % k_textureHistoryTileSize;
	UINT modY = texDesc.Height % k_textureHistoryTileSize;

	uint16_t j = 0;
	for (uint16_t i : textureDelta->m_tileIndices)
	{
		UINT numX = i % tileNumX;
		UINT numY = i / tileNumX;
		UINT offX = numX * k_textureHistoryTileSize;
		UINT offY = numY * k_textureHistoryTileSize;
		UINT sizeX = numX != tileNumX || modX == 0 ? k_textureHistoryTileSize : modX;
		UINT sizeY = numY != tileNumY || modX == 0 ? k_textureHistoryTileSize : modY;
		D3D11_BOX box {
			0, 0, 0,
			sizeX, sizeY, 1 };
		m_context->CopySubresourceRegion(
			texture.Get(),
			0,
			offX, offY, 0,
			textureDelta->m_textureTiles.Get(),
			j,
			&box
		);
		++j;
	}
}

void TextureHistory::updateConstantBuffer(
	TextureHistoryCB& cb)
{
	// Update constant buffer with numBLASInstances
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &cb, sizeof(TextureHistoryCB));
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}
