#pragma once

#include <wrl.h>
#include <d3d11.h>

#include <tiny_gltf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "primitiveData.hpp"

class Scene;
struct Texture;
struct Material;
class Primitive;
struct Transform;

using namespace Microsoft::WRL;
class GLTFModel
{
public:
	std::string path;
	GLTFModel(std::string path, ComPtr<ID3D11Device>& device, Scene* scene);
	~GLTFModel();

private:
	tinygltf::Model readGlb(const std::string& path);
	void processGlb(const tinygltf::Model& model);
	void processTextures(const tinygltf::Model& model);
	void processImages(const tinygltf::Model& model);
	void processMaterials(const tinygltf::Model& model);
	void processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Position>& verticies);
	void processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<TexCoords>& texCoords);
	void processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indicies);
	void processNormalsAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Normals>& normals);
	Transform getTransformFromNode(size_t meshIndex, const tinygltf::Model& model);
	ComPtr<ID3D11Device>& m_device;
	Scene* m_scene;
	std::unordered_map<uint32_t, uint32_t> m_textureIndex;
	std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_imageIndex;
	std::unordered_map<uint32_t, std::shared_ptr<Material>> m_materialIndex;
};
