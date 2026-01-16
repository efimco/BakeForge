#pragma once

#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <wrl.h>

#include "basePass.hpp"

using namespace Microsoft::WRL;

class Scene;
class ShaderManager;


class ObjectPicker : BasePass
{
public:
	ObjectPicker(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	float readBackID;
	void dispatchPick(const ComPtr<ID3D11ShaderResourceView>& srv, const uint32_t* mousePos, Scene* scene);

private:
	ComPtr<ID3D11Buffer> m_constantBuffer;
	ComPtr<ID3D11Buffer> m_structuredBuffer;
	ComPtr<ID3D11Buffer> m_stagingBuffer;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
};
