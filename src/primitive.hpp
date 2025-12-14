#pragma once
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include "primitiveData.hpp"
#include "material.hpp"
#include "sceneNode.hpp"
#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/tri.h>
#include <bvh/v2/node.h>
#include <bvh/v2/stream.h>
#include <bvh/v2/default_builder.h>

using namespace Microsoft::WRL;


using Scalar = float;
using Vec3 = bvh::v2::Vec<Scalar, 3>;
using BBox = bvh::v2::BBox<Scalar, 3>;
using Tri = bvh::v2::Tri<Scalar, 3>;
using Node = bvh::v2::Node<Scalar, 3>;
using Bvh = bvh::v2::Bvh<Node>;

class Primitive : public SceneNode
{
public:
	explicit Primitive(ComPtr<ID3D11Device> device);
	~Primitive() = default;
	Primitive(const Primitive&) = delete;
	Primitive& operator=(const Primitive&) = delete;
	Primitive(Primitive&&) = default;
	Primitive& operator=(Primitive&&) = default;

	void setVertexData(std::vector<InterleavedData>&& vertexData);
	void setIndexData(std::vector<uint32_t>&& indexData);

	const std::vector<InterleavedData>& getVertexData() const { return m_vertexData; }
	const std::vector<uint32_t>& getIndexData() const { return m_indexData; }
	const ComPtr<ID3D11Buffer>& getIndexBuffer() const { return m_indexBuffer; };
	const ComPtr<ID3D11Buffer>& getVertexBuffer() const { return m_vertexBuffer; };

	void buildBVH();
	const Bvh* getBVH() const;
	BBox getWorldBBox(glm::mat4 worldMatrix) const;
	const BBox* getLocalBBox() const;
	std::shared_ptr<Material> material;

private:
	std::vector<InterleavedData> m_vertexData;
	std::vector<uint32_t> m_indexData;
	std::vector<Tri> m_triangles;
	std::vector<BBox> m_bboxes;
	std::vector<Vec3> m_centers;
	std::unique_ptr<Bvh> m_bvh;
	std::unique_ptr<BBox> m_bbox;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11Buffer> m_indexBuffer;
	ComPtr<ID3D11Buffer> m_vertexBuffer;

};