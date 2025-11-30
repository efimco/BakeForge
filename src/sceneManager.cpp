#include "sceneManager.hpp"
#include "iostream"

static std::vector<std::unique_ptr<Primitive>> primitives;
static std::unordered_map<std::string, std::shared_ptr<Texture>> textures; // path + actual texture
static std::unordered_map<std::string, std::shared_ptr<Material>> materials;

static std::unordered_map<SceneNode*, bool> selectedNodes;
static SceneNode* activeNode = nullptr;

static std::vector<std::unique_ptr<Light>> lights;
static std::unordered_map<std::string, uint32_t> names;

static bool lightsAreDirty = true;

bool SceneManager::isNameUsed(std::string name)
{
	return names.find(name) != names.end();
}

uint32_t& SceneManager::getNameCounter(std::string name)
{
	return names[name];
}

std::unique_ptr<Primitive>& SceneManager::addPrimitive(std::unique_ptr<Primitive>&& primitive)
{
	primitives.push_back(std::move(primitive));
	return primitives.back();
}

std::unique_ptr<Light>& SceneManager::addLight(std::unique_ptr<Light>&& light)
{
	lights.push_back(std::move(light));
	return lights.back();
}

std::vector<std::unique_ptr<Primitive>>& SceneManager::getPrimitives()
{
	return primitives;
}

std::unique_ptr<Primitive>& SceneManager::getPrimitiveByID(size_t id)
{
	return primitives[id];
}

std::vector<std::unique_ptr<Light>>& SceneManager::getLights()
{
	return lights;
}

size_t SceneManager::getPrimitiveCount()
{
	return primitives.size();
}

std::shared_ptr<Texture> SceneManager::getTexture(std::string name)
{
	auto it = textures.find(name);
	if (it != textures.end())
	{
		return it->second;
	}
	return nullptr;
}

void SceneManager::addTexture(std::shared_ptr<Texture>&& texture)
{
	textures[texture->name] = std::move(texture);
}

std::shared_ptr<Material> SceneManager::getMaterial(std::string name)
{
	auto it = materials.find(name);
	if (it != materials.end())
	{
		return it->second;
	}
	return nullptr;
}

std::vector<std::string> SceneManager::getMaterialNames()
{
	std::vector<std::string> names;
	for (const auto& pair : materials)
	{
		names.push_back(pair.first);
	}
	return names;
}

SceneNode* SceneManager::getActiveNode()
{
	return activeNode;
}

void SceneManager::setActiveNode(SceneNode* node, bool addToSelection)
{
	if (!addToSelection)
	{
		clearSelectedNodes();
	}
	selectedNodes[node] = true;

	activeNode = node;
}

void SceneManager::deselectNode(SceneNode* node)
{
	if (activeNode == node)
	{
		activeNode = nullptr;
	}
	selectedNodes.erase(node);
}

void SceneManager::clearSelectedNodes()
{
	selectedNodes.clear();
}

bool SceneManager::isNodeSelected(SceneNode* node)
{
	return selectedNodes.find(node) != selectedNodes.end();
}

void SceneManager::addMaterial(std::shared_ptr<Material>&& material)
{
	materials[material->name] = std::move(material);
}

bool SceneManager::areLightsDirty()
{
	return lightsAreDirty;
}

void SceneManager::setLightsDirty(bool dirty)
{
	lightsAreDirty = dirty;
}


