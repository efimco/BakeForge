#pragma once
#include "primitiveData.hpp"
#include <vector>
#include "primitive.hpp"
#include "texture.hpp"
#include <memory>
#include "material.hpp"
#include "light.hpp"

namespace SceneManager
{
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
};