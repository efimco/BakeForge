#include "sceneManager.hpp"

static std::vector<std::unique_ptr<Primitive>> primitives;
static std::unordered_map<std::string, std::shared_ptr<Texture>> textures; // path + actual texture
static std::unordered_map<std::string, std::shared_ptr<Material>> materials;

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

