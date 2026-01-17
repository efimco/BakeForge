#pragma once

#include <d3d11.h>
#include <d3d11shader.h>
#include <vector>

#include <d3d11_4.h>
#include <wrl.h>

#include "sceneNode.hpp"
struct Material;

using namespace Microsoft::WRL;

struct Triangle;
struct Vertex;

struct SharedPrimitiveData
{
	std::vector<Vertex> vertexData;
	std::vector<uint32_t> indexData;
	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexStructuredBuffer;
	ComPtr<ID3D11Buffer> vertexStructuredBuffer;
	ComPtr<ID3D11ShaderResourceView> srv_indexStructuredBuffer;
	ComPtr<ID3D11ShaderResourceView> srv_vertexStructuredBuffer;
};

class Primitive : public SceneNode
{
public:
	explicit Primitive(ComPtr<ID3D11Device> device, std::string_view nodeName = "Primitive");
	~Primitive() override = default;
	Primitive(const Primitive&) = delete;
	Primitive(Primitive&&) = default;
	Primitive& operator=(const Primitive&) = delete;
	Primitive& operator=(Primitive&&) = delete;

	void setVertexData(std::vector<Vertex>&& vertexData) const;
	void setIndexData(std::vector<uint32_t>&& indexData) const;

	const std::vector<uint32_t>& getIndexData() const;
	ComPtr<ID3D11Buffer> getIndexBuffer() const;
	ComPtr<ID3D11Buffer> getVertexBuffer() const;
	ComPtr<ID3D11ShaderResourceView> getVertexStructuredBufferSRV() const;
	ComPtr<ID3D11ShaderResourceView> getIndexStructuredBufferSRV() const;


	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

	std::shared_ptr<Material> material;

private:
	std::shared_ptr<SharedPrimitiveData> m_sharedData;
	ComPtr<ID3D11Device> m_device;
};
