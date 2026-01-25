#pragma once

#include <d3d11.h>
#include <d3d11shader.h>
#include <vector>

#include <d3d11_4.h>
#include <wrl.h>

#include "bvhNode.hpp"
#include "sceneNode.hpp"

using namespace Microsoft::WRL;

struct Material;
struct Triangle;
struct Vertex;

struct SharedPrimitiveData
{
	std::vector<Vertex> vertexData;
	std::vector<uint32_t> indexData;
	std::vector<Triangle> triangles;
	std::vector<uint32_t> triangleIndices;
	std::vector<BVH::Node> bvhNodes;

	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> vertexBuffer;

	ComPtr<ID3D11Buffer> structuredTrisBuffer;
	ComPtr<ID3D11Buffer> structuredTrisIndicesBuffer;

	ComPtr<ID3D11ShaderResourceView> srv_structuredTrisBuffer;
	ComPtr<ID3D11ShaderResourceView> srv_structuredTrisIndicesBuffer;
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
	void fillTriangles();

	bool isVisible = true;

	const std::vector<uint32_t>& getIndexData() const;
	const std::vector<Vertex>& getVertexData() const;
	ComPtr<ID3D11Buffer> getIndexBuffer() const;
	ComPtr<ID3D11Buffer> getVertexBuffer() const;
	ComPtr<ID3D11ShaderResourceView> getTrisBufferSRV() const;
	ComPtr<ID3D11ShaderResourceView> getTrisIndicesBufferSRV() const;

	const std::vector<Triangle>& getTriangles() const;
	const std::vector<uint32_t>& getTriangleIndices() const;
	std::vector<BVH::Node>& getBVHNodes() const;

	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

	std::shared_ptr<Material> material;

private:
	void buildBVH();
	std::shared_ptr<SharedPrimitiveData> m_sharedData;
	ComPtr<ID3D11Device> m_device;
};
