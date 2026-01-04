#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <d3d11_4.h>
#include <wrl.h>
#include <future>

#include <bvh/v2/bvh.h>
#include <bvh/v2/thread_pool.h>

#include "sceneNode.hpp"
#include "sceneNodeHandle.hpp"
#include "utility/stringUnorderedMap.hpp"
#include "gltfImporter.hpp"

class Primitive;
struct Texture;
struct Material;
class Light;
class Camera;

using Scalar = float;
using Vec3 = bvh::v2::Vec<Scalar, 3>;
using BBox = bvh::v2::BBox<Scalar, 3>;
using Node = bvh::v2::Node<Scalar, 3>;
using Bvh = bvh::v2::Bvh<Node>;
using namespace Microsoft::WRL;

class Scene : public SceneNode
{
public:
	explicit Scene(std::string_view name = "Default Scene");
	~Scene() override = default;

	SceneNodeHandle findHandleOfNode(SceneNode* node) const;
	SceneNode* getNodeByHandle(SceneNodeHandle handle);
	SceneNode* getRootNode() const;
	bool isNameUsed(std::string_view name) const;
	uint32_t& getNameCounter(std::string_view name);

	void addPrimitive(Primitive* primitive);
	SceneUnorderedMap<Primitive*>& getPrimitives();
	Primitive* getPrimitiveByID(size_t id);
	size_t getPrimitiveCount() const;

	void addLight(Light* light);
	Light* getLightByID(size_t id);
	SceneUnorderedMap<Light*>& getLights();

	void addCamera(Camera* camera);

	std::shared_ptr<Texture> getTexture(std::string_view name);
	void addTexture(std::shared_ptr<Texture> texture);

	std::shared_ptr<Material> getMaterial(std::string_view name);
	void addMaterial(std::shared_ptr<Material> material);
	std::vector<std::string> getMaterialNames() const;

	SceneNode* getActiveNode();
	int32_t getActiveNodeID();
	int32_t getActivePrimitiveID();
	void setActiveNode(SceneNode* node, bool addToSelection = false);
	void deselectNode(SceneNode* node);
	void clearSelectedNodes();
	bool isNodeSelected(SceneNode* node);

	bool areLightsDirty();
	void setLightsDirty(bool dirty = true);
	void setActiveCamera(Camera* camera);

	void deleteNode(SceneNode* node);
	SceneNode* adoptClonedNode(
		std::unique_ptr<SceneNode>&& clonedNode,
		SceneNodeHandle preferredHandle = SceneNodeHandle::invalidHandle());
	Camera* getActiveCamera();

	void buildSceneBVH();
	void markSceneBVHDirty();
	bool isSceneBVHDirty() const;
	void rebuildSceneBVHIfDirty();
	void validateName(SceneNode* node);
	const Bvh* getSceneBVH() const;

	void importModel(std::string filepath, ComPtr<ID3D11Device> device);
	void updateAsyncImport(); // called each frame
	bool isImporting() const { return m_isImporting; }
	std::shared_ptr<ImportProgress> getImportProgress() const { return m_importProgress; }

private:
	SceneNode m_rootNode;
	SceneUnorderedMap<Primitive*> m_primitives;
	SceneUnorderedMap<Light*> m_lights;
	SceneUnorderedMap<Camera*> m_cameras;
	StringUnorderedMap<std::shared_ptr<Texture>> m_textures; // path + actual texture
	StringUnorderedMap<std::shared_ptr<Material>> m_materials; // path + actual material
	StringUnorderedMap<uint32_t> m_nodeNames;

	std::unique_ptr<Bvh> m_sceneBVH;
	std::vector<BBox> m_primBboxes; // World-space bboxes for each primitive
	std::vector<Vec3> m_primCenters; // Centers of each primitive bbox

	bool m_isImporting = false;
	std::future<AsyncImportResult> m_importFuture;
	std::shared_ptr<ImportProgress> m_importProgress;
	ComPtr<ID3D11Device> m_importDevice;

	std::unique_ptr<bvh::v2::ThreadPool> m_threadPool;

	std::unordered_map<SceneNode*, bool> m_selectedNodes;

	Camera* m_activeCamera = nullptr;
	SceneNode* m_activeNode = nullptr;

	bool m_lightsAreDirty = false;
	bool m_sceneBVHDirty = true;
};
