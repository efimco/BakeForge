#include <iostream>
#include "gltfImporter.hpp"
#include "primitive.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "scene.hpp"
#include <memory>
#include <glm/gtc/type_ptr.hpp>



GLTFModel::GLTFModel(std::string path, ComPtr<ID3D11Device>& device, Scene* scene) : m_device(device), m_scene(scene)
{
	const tinygltf::Model model = readGlb(path);
	processGlb(model);

}

GLTFModel::~GLTFModel()
{
	std::cout << "GLTFModel Destructor Called" << std::endl;
}

tinygltf::Model GLTFModel::readGlb(const std::string& path)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err;
	std::string warn;
	std::cout << "Loading glTF file: " << path << std::endl;
	bool ret = false;
	if (path.ends_with("gltf"))
	{
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
	}
	else
	{
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
	}

	if (!warn.empty())
		std::cout << "Warn: " << warn << std::endl;
	if (!err.empty())
		std::cout << "Err: " << err << std::endl;
	if (!ret)
		std::cout << "Failed to parse glTF" << std::endl;
	std::cout << "Loaded " << path << std::endl;
	return model;
}

void GLTFModel::processGlb(const tinygltf::Model& model)
{
	std::cout << "Processing GLTF model with " << model.meshes.size() << " meshes" << std::endl;
	processTextures(model);
	processImages(model);
	processMaterials(model);
	for (const auto& mesh : model.meshes)
	{
		for (const auto& gltfPrimitive : mesh.primitives)
		{
			std::vector<Position> posBuffer;
			std::vector<TexCoords> texCoordsBuffer;
			std::vector<Normals> normalBuffer;
			std::vector<uint32_t> indices;

			processPosAttribute(model, mesh, gltfPrimitive, posBuffer);
			processTexCoordAttribute(model, mesh, gltfPrimitive, texCoordsBuffer);
			processIndexAttrib(model, mesh, gltfPrimitive, indices);
			processNormalsAttribute(model, mesh, gltfPrimitive, normalBuffer);
			std::vector<InterleavedData> vertexData;
			const auto numVert = posBuffer.size();
			for (int i = 0; i < numVert; i++)
			{
				InterleavedData interData;
				interData.position = posBuffer[i];
				interData.texCoords = texCoordsBuffer[i];
				interData.normals = normalBuffer[i];
				vertexData.push_back(interData);
			}
			size_t meshIndex = &mesh - &model.meshes[0];
			Transform transform = getTransformFromNode(meshIndex, model);
			std::unique_ptr<Primitive> primitive = std::make_unique<Primitive>(m_device);
			primitive->setVertexData(std::move(vertexData));
			primitive->setIndexData(std::move(indices));
			primitive->material = m_materialIndex[gltfPrimitive.material];
			primitive->transform = transform;

			if (m_scene->isNameUsed(model.nodes[meshIndex].name))
			{
				m_scene->getNameCounter(model.nodes[meshIndex].name)++;
				primitive->name = model.nodes[meshIndex].name + "." + std::to_string(m_scene->getNameCounter(model.nodes[meshIndex].name));
			}
			else
			{
				m_scene->getNameCounter(model.nodes[meshIndex].name) = 0;
				primitive->name = model.nodes[meshIndex].name;
			}
			// primitive->buildBVH();
			m_scene->addPrimitive(primitive.get());
			m_scene->addChild(std::move(primitive));

			std::cout << "Added primitive. Total primitives now: " << m_scene->getPrimitiveCount() << std::endl;
		}
	}
}

void GLTFModel::processTextures(const tinygltf::Model& model)
{
	for (int i = 0; i < model.textures.size(); i++)
	{
		m_textureIndex[i] = model.textures[i].source;
	}
}

void GLTFModel::processImages(const tinygltf::Model& model)
{
	for (int i = 0; i < model.images.size(); i++)
	{
		std::string name = model.images[i].name;
		if (name.empty())
		{
			name = model.images[i].uri;
		}
		if (m_scene->getTexture(name) == nullptr)
		{
			std::shared_ptr<Texture> texture = std::make_shared<Texture>(model.images[i], m_device);
			m_scene->addTexture(std::move(texture));
		}
		m_imageIndex[i] = m_scene->getTexture(name);
	}
}

void GLTFModel::processMaterials(const tinygltf::Model& model)
{
	for (int i = 0; i < model.materials.size(); i++)
	{
		auto& material = model.materials[i];
		std::shared_ptr<Material> mat = std::make_shared<Material>();
		if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
		{
			mat->albedo = m_imageIndex[m_textureIndex[material.pbrMetallicRoughness.baseColorTexture.index]];
		}

		if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
		{
			mat->metallicRoughness = m_imageIndex[m_textureIndex[material.pbrMetallicRoughness.metallicRoughnessTexture.index]];
		}
		if (material.normalTexture.index != -1)
		{
			mat->normal = m_imageIndex[m_textureIndex[material.normalTexture.index]];
		}
		mat->name = material.name;
		if (material.pbrMetallicRoughness.baseColorFactor.size() == 4)
		{
			mat->albedoColor = glm::vec4(
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3])
			);
		}

		mat->roughnessValue = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
		mat->metallicValue = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);

		auto name = material.name;
		if (m_scene->getMaterial(name) == nullptr)
		{
			m_scene->addMaterial(std::move(mat));
		}
		m_materialIndex[i] = m_scene->getMaterial(name);
	}
	if (model.materials.empty())
	{
		std::cerr << "No materials found in the model." << std::endl;
	}
	else
	{
		std::cout << "Processed " << model.materials.size() << " materials." << std::endl;
	}
}

void GLTFModel::processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Position>& verticies)
{
	if (primitive.attributes.find("POSITION") == primitive.attributes.end())
	{
		std::cerr << "No POSITION attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
	const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
	const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

	const unsigned char* posDataPtr = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
	const float* posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	size_t vertexCount = posAccessor.count;
	int components = (posAccessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		Position position(-INFINITY, -INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) position.x = posFloatPtr[i * components + j];
			else if (j == 1) position.y = posFloatPtr[i * components + j];
			else if (j == 2) position.z = posFloatPtr[i * components + j];
		}
		verticies.push_back(position);
	}
}

void GLTFModel::processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<TexCoords>& texCoords)
{
	if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
	{
		std::cerr << "No TEXCOORD_0 attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* posDataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const float* posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	size_t vertexCount = accessor.count;
	int components = (accessor.type == TINYGLTF_TYPE_VEC2) ? 2 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		TexCoords texCoord(-INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) texCoord.u = posFloatPtr[i * components + j];
			else if (j == 1) texCoord.v = posFloatPtr[i * components + j];
		}
		texCoords.push_back(texCoord);
	}
}

void GLTFModel::processNormalsAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Normals>& normals)
{
	if (primitive.attributes.find("NORMAL") == primitive.attributes.end())
	{
		std::cerr << "No NORMAL attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const float* floatPtr = reinterpret_cast<const float*>(dataPtr);
	size_t vertexCount = accessor.count;
	int components = (accessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		Normals normal(-INFINITY, -INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) normal.nx = floatPtr[i * components + j];
			else if (j == 1) normal.ny = floatPtr[i * components + j];
			else if (j == 2) normal.nz = floatPtr[i * components + j];
		}
		normals.push_back(normal);
	}
}

Transform GLTFModel::getTransformFromNode(size_t meshIndex, const tinygltf::Model& model)
{
	Transform transform;
	transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
	tinygltf::Node node;
	for (const auto& n : model.nodes)
	{
		if (n.mesh == static_cast<int>(meshIndex))
		{
			node = n;
			break;
		}
	}
	if (node.translation.size() != 0)
	{
		transform.position = glm::vec3(node.translation[0], node.translation[1],
			node.translation[2]);
	}

	transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.rotation.size() != 0)
	{
		glm::quat quatRot(static_cast<float>(node.rotation[3]),
			static_cast<float>(node.rotation[0]),
			static_cast<float>(node.rotation[1]),
			static_cast<float>(node.rotation[2]));
		transform.rotation = glm::eulerAngles(quatRot);
	}

	transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.scale.size() != 0)
	{
		transform.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
	}
	transform.matrix = glm::mat4(1.0f);
	transform.updateMatrix();
	return transform;
}


void GLTFModel::processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indicies)
{
	if (primitive.indices < 0)
	{
		std::cerr << "No indices found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
	const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
	const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];


	const void* pIndexData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
	size_t indexCount = indexAccessor.count;
	if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		const uint16_t* indices = reinterpret_cast<const uint16_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		const uint32_t* indices = reinterpret_cast<const uint32_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
	{
		const uint8_t* indices = reinterpret_cast<const uint8_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
}
