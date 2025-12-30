#pragma once

#include <d3d11_1.h>
#include <wrl.h>

using namespace Microsoft::WRL;

struct GBufferTextures
{

	ComPtr<ID3D11ShaderResourceView> albedoSRV;
	ComPtr<ID3D11ShaderResourceView> metallicRoughnessSRV;
	ComPtr<ID3D11ShaderResourceView> normalSRV;
	ComPtr<ID3D11ShaderResourceView> positionSRV;
	ComPtr<ID3D11ShaderResourceView> objectIDSRV;

};