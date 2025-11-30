#pragma once

#include "sceneNode.hpp"
#include "primitive.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "light.hpp"
#include "camera.hpp"


class Scene : public SceneNode
{
public:
	Scene(std::string name = "Default Scene");
	~Scene() = default;

	SceneNode* getRootNode() { return children.front(); }
	bool isNameUsed(std::string name);
	uint32_t& getNameCounter(std::string name);

	std::unique_ptr<Primitive>& addPrimitive(std::unique_ptr<Primitive>&& primitive);
	std::vector<std::unique_ptr<Primitive>>& getPrimitives();
	std::unique_ptr<Primitive>& getPrimitiveByID(size_t id);
	size_t getPrimitiveCount();

	std::unique_ptr<Light>& addLight(std::unique_ptr<Light>&& light);
	std::vector<std::unique_ptr<Light>>& getLights();

	std::shared_ptr<Texture> getTexture(std::string name);
	void addTexture(std::shared_ptr<Texture>&& texture);

	std::shared_ptr<Material> getMaterial(std::string name);
	void addMaterial(std::shared_ptr<Material>&& material);
	std::vector<std::string> getMaterialNames();

	SceneNode* getActiveNode();
	void setActiveNode(SceneNode* node, bool addToSelection = false);
	void deselectNode(SceneNode* node);
	void clearSelectedNodes();
	bool isNodeSelected(SceneNode* node);


	bool areLightsDirty();
	void setLightsDirty(bool dirty);

private:
	SceneNode m_rootNode;
	std::vector<std::unique_ptr<Primitive>> m_primitives;
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures; // path + actual texture
	std::unordered_map<std::string, std::shared_ptr<Material>> m_materials; // path + actual material
	std::vector<std::unique_ptr<Light>> m_lights;

	SceneNode* m_activeNode = nullptr;

	std::unordered_map<SceneNode*, bool> m_selectedNodes;

	bool m_lightsAreDirty;

	std::unordered_map<std::string, uint32_t> m_nodeNames;

};