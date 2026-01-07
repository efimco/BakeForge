#pragma once

#include <memory>

#include <d3d11_4.h>
#include <wrl.h>
#include <glm/glm.hpp>

using namespace Microsoft::WRL;

class Scene;
class ShaderManager;


class ObjectPicker
{
public:
	ObjectPicker(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	uint32_t readBackID;
	void dispatchPick(const ComPtr<ID3D11ShaderResourceView>& srv, const uint32_t* mousePos, Scene* scene);

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11Buffer> m_structuredBuffer;
	ComPtr<ID3D11Buffer> m_stagingBuffer;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
	std::unique_ptr<ShaderManager> m_shaderManager;

};
