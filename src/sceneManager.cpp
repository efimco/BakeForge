#include "sceneManager.hpp"
#include "iostream"

static std::vector<std::unique_ptr<Primitive>> primitives;
static std::unordered_map<std::string, std::shared_ptr<Texture>> textures; // path + actual texture
static std::unordered_map<std::string, std::shared_ptr<Material>> materials;

static std::unordered_map<Primitive*, bool> selectedPrimitives;
static std::unordered_map<SceneNode*, bool> selectedNodes;
static SceneNode* selectedNode = nullptr;

static std::vector<std::unique_ptr<Light>> lights;
static std::unordered_map<std::string, uint32_t> names;

static bool lightsAreDirty = true;

static bool transformsAreDirty = true;


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

SceneNode* SceneManager::getSelectedNode()
{
	return selectedNode;
}

void SceneManager::setSelectedNode(SceneNode* node, bool addToSelection)
{
	if (addToSelection)
	{
		selectedNodes[node] = true;
	}
	else
	{
		clearSelectedNodes();
		selectedNodes[node] = true;
	}
	selectedNode = node;
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

void SceneManager::selectPrimitive(Primitive* primitive, bool addToSelection)
{
	const auto& it = selectedPrimitives.find(primitive);
	if (it == selectedPrimitives.end())
	{
		if (!addToSelection)
		{
			clearSelectedPrimitives();
		}
		selectedPrimitives[primitive] = true;
		selectedNode = primitive;
		selectedNodes[primitive] = true;
	}

}

void SceneManager::selectPrimitive(uint32_t id, bool addToSelection)
{
	const auto& it = selectedPrimitives.find(primitives[id].get());
	if (it == selectedPrimitives.end())
	{
		Primitive* primitive = primitives[id].get();
		if (!addToSelection)
		{
			clearSelectedPrimitives();
		}
		selectedPrimitives[primitive] = true;
		selectedNode = primitive;
		selectedNodes[primitive] = true;
	}
}

void SceneManager::deselectPrimitive(Primitive* primitive)
{
	const auto& it = selectedPrimitives.find(primitive);
	if (it != selectedPrimitives.end())
	{
		if (selectedNode == primitive)
		{
			selectedNode = nullptr;
		}
		selectedPrimitives.erase(it);
		selectedNodes.erase(primitive);
		if (selectedNode == primitive)
		{
			selectedNode = nullptr;
		}
	}
}

void SceneManager::deselectPrimitive(uint32_t id)
{
	const auto& it = selectedPrimitives.find(primitives[id].get());
	if (it != selectedPrimitives.end())
	{
		selectedPrimitives.erase(it);
		selectedNodes.erase(primitives[id].get());
		if (selectedNode == primitives[id].get())
		{
			selectedNode = nullptr;
		}
	}
}

bool SceneManager::isPrimitiveSelected(Primitive* primitive)
{
	if (selectedPrimitives.find(primitive) == selectedPrimitives.end())
	{

		return false;
	}
	if (selectedPrimitives[primitive] == false)
	{
		return false;
	}
	else
	{
		return true;
	}
	return false;
}

bool SceneManager::isPrimitiveSelected(uint32_t id)
{
	if (selectedPrimitives.find(primitives[id].get()) == selectedPrimitives.end())
	{

		return false;
	}
	if (selectedPrimitives[primitives[id].get()] == false)
	{
		return false;
	}
	else
	{
		return true;
	}
	return false;
}

void SceneManager::clearSelectedPrimitives()
{
	selectedPrimitives.clear();
	selectedNodes.clear();
	selectedNode = nullptr;

}

bool SceneManager::areLightsDirty()
{
	return lightsAreDirty;
}

void SceneManager::setLightsDirty(bool dirty)
{
	lightsAreDirty = dirty;
}

void SceneManager::setTransformsDirty(bool dirty)
{
	transformsAreDirty = dirty;
}

bool SceneManager::areTransformsDirty()
{
	return transformsAreDirty;
}

