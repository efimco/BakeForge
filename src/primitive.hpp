#pragma once

#include <vector>

#include <d3d11_4.h>
#include <wrl.h>

#include "primitiveData.hpp"
#include "sceneNode.hpp"

struct Material;

using namespace Microsoft::WRL;


struct SharedPrimitiveData
{
	std::vector<InterleavedData> vertexData;
	std::vector<uint32_t> indexData;
	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> vertexBuffer;
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

	void setVertexData(std::vector<InterleavedData>&& vertexData) const;
	void setIndexData(std::vector<uint32_t>&& indexData) const;

	const std::vector<uint32_t>& getIndexData() const;
	const ComPtr<ID3D11Buffer>& getIndexBuffer() const;
	const ComPtr<ID3D11Buffer>& getVertexBuffer() const;

	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;
	void setSharedPrimitiveData(std::shared_ptr<SharedPrimitiveData> sharedData);

	std::shared_ptr<Material> material;

private:
	std::shared_ptr<SharedPrimitiveData> m_sharedData;
	ComPtr<ID3D11Device> m_device;
};
