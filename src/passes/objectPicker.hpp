#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <glm/glm.hpp>
#include "shaderManager.hpp"

using namespace Microsoft::WRL;


struct cbPicking
{
	uint32_t mousePosX;
	uint32_t mousePosY;
	uint32_t padding[2];
};

class ObjectPicker
{
public:
	ObjectPicker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	uint32_t readBackID;
	void dispatchPick(const ComPtr<ID3D11ShaderResourceView>& srv, uint32_t* mousePos);
private:
	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;
	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11Buffer> m_structuredBuffer;
	ComPtr<ID3D11Buffer> m_stagingBuffer;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
	ShaderManager* m_shaderManager;

};