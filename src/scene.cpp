#include "scene.hpp"

Scene::Scene(std::string name)
{
	this->name = name;
}

bool Scene::isNameUsed(std::string name)
{
	return m_nodeNames.find(name) != m_nodeNames.end();
}

uint32_t& Scene::getNameCounter(std::string name)
{
	return m_nodeNames[name];
}

std::unique_ptr<Primitive>& Scene::addPrimitive(std::unique_ptr<Primitive>&& primitive)
{
	m_primitives.push_back(std::move(primitive));
	return m_primitives.back();
}

std::unique_ptr<Light>& Scene::addLight(std::unique_ptr<Light>&& light)
{
	m_lights.push_back(std::move(light));
	addChild(m_lights.back().get());
	return m_lights.back();
}

std::vector<std::unique_ptr<Primitive>>& Scene::getPrimitives()
{
	return m_primitives;
}

std::unique_ptr<Primitive>& Scene::getPrimitiveByID(size_t id)
{
	return m_primitives[id];
}

std::vector<std::unique_ptr<Light>>& Scene::getLights()
{
	return m_lights;
}

size_t Scene::getPrimitiveCount()
{
	return m_primitives.size();
}

std::shared_ptr<Texture> Scene::getTexture(std::string name)
{
	auto it = m_textures.find(name);
	if (it != m_textures.end())
	{
		return it->second;
	}
	return nullptr;
}

void Scene::addTexture(std::shared_ptr<Texture>&& texture)
{
	m_textures[texture->name] = std::move(texture);
}

std::shared_ptr<Material> Scene::getMaterial(std::string name)
{
	auto it = m_materials.find(name);
	if (it != m_materials.end())
	{
		return it->second;
	}
	return nullptr;
}

std::vector<std::string> Scene::getMaterialNames()
{
	std::vector<std::string> materialNames;
	for (const auto& pair : m_materials)
	{
		materialNames.push_back(pair.first);
	}
	return materialNames;
}

SceneNode* Scene::getActiveNode()
{
	return m_activeNode;
}

void Scene::setActiveNode(SceneNode* node, bool addToSelection)
{
	if (!addToSelection)
	{
		clearSelectedNodes();
	}
	m_selectedNodes[node] = true;

	m_activeNode = node;
}

void Scene::deselectNode(SceneNode* node)
{
	if (m_activeNode == node)
	{
		m_activeNode = nullptr;
	}
	m_selectedNodes.erase(node);
}

void Scene::clearSelectedNodes()
{
	m_selectedNodes.clear();
}

bool Scene::isNodeSelected(SceneNode* node)
{
	return m_selectedNodes.find(node) != m_selectedNodes.end();
}

void Scene::addMaterial(std::shared_ptr<Material>&& material)
{
	m_materials[material->name] = std::move(material);
}

bool Scene::areLightsDirty()
{
	return m_lightsAreDirty;
}

void Scene::setLightsDirty(bool dirty)
{
	m_lightsAreDirty = dirty;
}


