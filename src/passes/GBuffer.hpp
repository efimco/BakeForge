#pragma once
#include <d3d11_1.h>
#include <wrl.h>
#include "camera.hpp"
#include <glm/glm.hpp>
#include "shaderManager.hpp"
#include "primitive.hpp"
#include "scene.hpp"

using namespace Microsoft::WRL;

class GBuffer
{
public:
	GBuffer(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~GBuffer() = default;

	void draw(const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPosition,
		Scene* scene,
		ComPtr<ID3D11DepthStencilView>& dsv);
	void update(const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPosition,
		Scene* scene,
		int objectID,
		Primitive* prim);
	void createOrResize();

	const ComPtr<ID3D11ShaderResourceView>& getAlbedoSRV() const;
	const ComPtr<ID3D11ShaderResourceView>& getMetallicRoughnessSRV() const;
	const ComPtr<ID3D11ShaderResourceView>& getNormalSRV() const;
	const ComPtr<ID3D11ShaderResourceView>& getPositionSRV() const;
	const ComPtr<ID3D11ShaderResourceView>& getObjectIDSRV() const;

private:


	ComPtr<ID3D11Texture2D> t_albedo;
	ComPtr<ID3D11Texture2D> t_metallicRoughness;
	ComPtr<ID3D11Texture2D> t_normal;
	ComPtr<ID3D11Texture2D> t_position;
	ComPtr<ID3D11Texture2D> t_objectID;

	ComPtr<ID3D11RenderTargetView> rtv_albedo;
	ComPtr<ID3D11RenderTargetView> rtv_metallicRoughness;
	ComPtr<ID3D11RenderTargetView> rtv_normal;
	ComPtr<ID3D11RenderTargetView> rtv_position;
	ComPtr<ID3D11RenderTargetView> rtv_objectID;

	ComPtr<ID3D11ShaderResourceView> srv_albedo;
	ComPtr<ID3D11ShaderResourceView> srv_metallicRoughness;
	ComPtr<ID3D11ShaderResourceView> srv_normal;
	ComPtr<ID3D11ShaderResourceView> srv_position;
	ComPtr<ID3D11ShaderResourceView> srv_objectID;

	ComPtr<ID3D11Buffer> m_constantbuffer;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11InputLayout> m_inputLayout;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	ID3D11RenderTargetView* m_rtvs[5];

	const ComPtr<ID3D11Device>& m_device;
	const ComPtr<ID3D11DeviceContext>& m_context;

	std::unique_ptr<ShaderManager> m_shaderManager;
};
