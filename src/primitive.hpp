#pragma once

#include <vector>

#include <d3d11_4.h>
#include <wrl.h>
#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/tri.h>
#include <bvh/v2/node.h>
#include <bvh/v2/stream.h>
#include <bvh/v2/default_builder.h>

#include "primitiveData.hpp"
#include "sceneNode.hpp"

struct Material;

using namespace Microsoft::WRL;


using Scalar = float;
using Vec3 = bvh::v2::Vec<Scalar, 3>;
using BBox = bvh::v2::BBox<Scalar, 3>;
using Tri = bvh::v2::Tri<Scalar, 3>;
using Node = bvh::v2::Node<Scalar, 3>;
using Bvh = bvh::v2::Bvh<Node>;

struct SharedPrimitiveData
{
	std::vector<InterleavedData> vertexData;
	std::vector<uint32_t> indexData;
	std::vector<Tri> triangles;
	std::vector<BBox> bboxes;
	std::vector<Vec3> centers;
	std::unique_ptr<Bvh> bvh;
	std::unique_ptr<BBox> bbox;
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

	void buildBVH() const;
	const Bvh* getBVH() const;
	BBox getWorldBBox(const glm::mat4& worldMatrix) const;
	const BBox* getLocalBBox() const;
	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;
	void setSharedPrimitiveData(std::shared_ptr<SharedPrimitiveData> sharedData);

	std::shared_ptr<Material> material;

private:
	std::shared_ptr<SharedPrimitiveData> m_sharedData;
	ComPtr<ID3D11Device> m_device;

};
