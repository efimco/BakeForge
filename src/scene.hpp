#pragma once

#include "sceneNode.hpp"
#include "sceneNodeHandle.hpp"
#include <bvh/v2/bvh.h>
#include <bvh/v2/thread_pool.h>
#include <memory>

#include <string>
#include <string_view>
#include <unordered_map>

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

template <typename T>
using SceneUnorderedMap = std::unordered_map<SceneNodeHandle, T>;

template <typename T>
using StringUnorderedMap = std::unordered_map<std::string, T>;

class Scene : public SceneNode
{
public:
	Scene(std::string name = "Default Scene");
	~Scene() = default;

	SceneNodeHandle getHandleOfNode(SceneNode* node);
	SceneNode* getNodeByHandle(SceneNodeHandle handle);
	SceneNode* getRootNode();
	bool isNameUsed(std::string name);
	uint32_t& getNameCounter(std::string name);

	void addPrimitive(Primitive* primitive);
	SceneUnorderedMap<Primitive*>& getPrimitives();
	Primitive* getPrimitiveByID(size_t id);
	size_t getPrimitiveCount();

	void addLight(Light* light);
	SceneUnorderedMap<Light*>& getLights();

	std::shared_ptr<Texture> getTexture(std::string name);
	void addTexture(std::shared_ptr<Texture>&& texture);

	std::shared_ptr<Material> getMaterial(std::string name);
	void addMaterial(std::shared_ptr<Material>&& material);
	std::vector<std::string> getMaterialNames();

	SceneNode* getActiveNode();
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
	bool isSceneBVHDirty();
	void rebuildSceneBVHIfDirty();
	void validateName(SceneNode* node);
	const Bvh* getSceneBVH() const;

private:
	SceneNode m_rootNode;
	SceneUnorderedMap<Primitive*> m_primitives;
	SceneUnorderedMap<Light*> m_lights;
	SceneUnorderedMap<Camera*> m_cameras;
	StringUnorderedMap<std::shared_ptr<Texture>> m_textures; // path + actual texture
	StringUnorderedMap<std::shared_ptr<Material>> m_materials; // path + actual material
	Camera* m_activeCamera = nullptr;

	SceneNode* m_activeNode = nullptr;

	std::unordered_map<SceneNode*, bool> m_selectedNodes;

	bool m_lightsAreDirty = false;

	StringUnorderedMap<uint32_t> m_nodeNames;

	std::unique_ptr<Bvh> m_sceneBVH;
	std::vector<BBox> m_primBboxes; // World-space bboxes for each primitive
	std::vector<Vec3> m_primCenters; // Centers of each primitive bbox
	bool m_sceneBVHDirty = true;
	std::unique_ptr<bvh::v2::ThreadPool> m_threadPool;
};
