#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


#include <d3d11_4.h>
#include <tiny_gltf.h>
#include <wrl.h>

#include "primitiveData.hpp"

using namespace Microsoft::WRL;

class Scene;
struct Texture;
struct Material;
class Primitive;
struct Transform;

struct ImportProgress
{
	std::atomic<float> progress = 0.0f;
	std::atomic<bool> isCompleted = false;
	std::atomic<bool> hasFailed = false;
	std::string message;

	std::string currentStage;
	std::mutex stageMutex;

	void setStage(const std::string& stage)
	{
		std::lock_guard<std::mutex> lock(stageMutex);
		currentStage = stage;
	}

	std::string getStage()
	{
		std::lock_guard<std::mutex> lock(stageMutex);
		return currentStage;
	}
};

struct AsyncImportResult
{
	std::shared_ptr<ImportProgress> progress;
	ComPtr<ID3D11CommandList> commandList;
	std::vector<std::unique_ptr<Primitive>> primitives;
	std::vector<std::shared_ptr<Texture>> textures;
	std::vector<std::shared_ptr<Material>> materials;
};


class GLTFModel
{
public:
	std::string path;
	GLTFModel(const std::string& path, ComPtr<ID3D11Device> device, Scene* scene);
	~GLTFModel();

	static std::future<AsyncImportResult>
	importModelAsync(const std::string& path, ComPtr<ID3D11Device> device, std::shared_ptr<ImportProgress> progress);

	static void
	finalizeAsyncImport(AsyncImportResult&& importResult, ComPtr<ID3D11DeviceContext> immediateContext, Scene* scene);

private:
	GLTFModel(const std::string& path,
			  ComPtr<ID3D11Device> device,
			  ComPtr<ID3D11DeviceContext> deferredContext,
			  std::shared_ptr<ImportProgress> progress);

	static tinygltf::Model readGlb(const std::string& path);
	void processGlb(const tinygltf::Model& model);
	void processTextures(const tinygltf::Model& model);
	void processImages(const tinygltf::Model& model);
	void processMaterials(const tinygltf::Model& model);

	static void processPosAttribute(const tinygltf::Model& model,
									const tinygltf::Mesh& mesh,
									const tinygltf::Primitive& primitive,
									std::vector<Position>& verticies);

	static void processTexCoordAttribute(const tinygltf::Model& model,
										 const tinygltf::Mesh& mesh,
										 const tinygltf::Primitive& primitive,
										 std::vector<TexCoords>& texCoords);

	static void processIndexAttrib(const tinygltf::Model& model,
								   const tinygltf::Mesh& mesh,
								   const tinygltf::Primitive& primitive,
								   std::vector<uint32_t>& indicies);

	static void processNormalsAttribute(const tinygltf::Model& model,
										const tinygltf::Mesh& mesh,
										const tinygltf::Primitive& primitive,
										std::vector<Normal>& normal);

	static Transform getTransformFromNode(size_t meshIndex, const tinygltf::Model& model);
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deferredContext = nullptr;
	Scene* m_scene = nullptr;
	std::shared_ptr<ImportProgress> m_progress;

	std::unordered_map<uint32_t, uint32_t> m_textureIndex;
	std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_imageIndex;
	std::unordered_map<uint32_t, std::shared_ptr<Material>> m_materialIndex;

	std::vector<std::unique_ptr<Primitive>> m_pendingPrimitives;
	std::vector<std::shared_ptr<Texture>> m_pendingTextures;
	std::vector<std::shared_ptr<Material>> m_pendingMaterials;
};
