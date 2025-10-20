#include "sceneManager.hpp"
#include "iostream"
static std::vector<std::unique_ptr<Primitive>> primitives;
static std::unordered_map<std::string, std::shared_ptr<Texture>> textures; // path + actual texture
static std::unordered_map<std::string, std::shared_ptr<Material>> materials;

static std::unordered_map<Primitive*, bool> selectedPrimitives;

std::unique_ptr<Primitive>& SceneManager::addPrimitive(std::unique_ptr<Primitive>&& primitive)
{
	primitives.push_back(std::move(primitive));
	return primitives.back();
}

std::vector<std::unique_ptr<Primitive>>& SceneManager::getPrimitives()
{
	return primitives;
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

void SceneManager::addMaterial(std::shared_ptr<Material>&& material)
{
	materials[material->name] = std::move(material);
}

void SceneManager::selectPrimitive(Primitive* primitive)
{
	const auto& it = selectedPrimitives.find(primitive);
	if (it == selectedPrimitives.end())
	{
		selectedPrimitives[primitive] = true;
	}
}

void SceneManager::selectPrimitive(uint32_t id)
{
	const auto& it = selectedPrimitives.find(primitives[id].get());
	if (it == selectedPrimitives.end())
	{
		selectedPrimitives[primitives[id].get()] = true;
	}
}

void SceneManager::deselectPrimitive(Primitive* primitive)
{
	const auto& it = selectedPrimitives.find(primitive);
	if (it != selectedPrimitives.end())
	{
		selectedPrimitives.erase(it);
	}
}

void SceneManager::deselectPrimitive(uint32_t id)
{
	const auto& it = selectedPrimitives.find(primitives[id].get());
	if (it != selectedPrimitives.end())
	{
		selectedPrimitives.erase(it);
	}
}

bool SceneManager::isPrimitiveSelected(Primitive* primitive)
{
	return selectedPrimitives.find(primitive) != selectedPrimitives.end();
}

void SceneManager::clearSelectedPrimitives()
{
	selectedPrimitives.clear();
}
