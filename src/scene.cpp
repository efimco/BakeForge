#include "scene.hpp"
#include <iostream>
#include "chrono"
#include "primitive.hpp"
#include "light.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "camera.hpp"

Scene::Scene(std::string name)
{
	this->name = name;
}

SceneNode* Scene::getRootNode()
{
	return children.front().get();
}

bool Scene::isNameUsed(std::string name)
{
	return m_nodeNames.find(name) != m_nodeNames.end();
}

uint32_t& Scene::getNameCounter(std::string name)
{
	return m_nodeNames[name];
}

void Scene::addPrimitive(Primitive* primitive)
{
	m_primitives.push_back(primitive);
}

void Scene::addLight(Light* light)
{
	m_lights.push_back(light);
}

std::vector<Primitive*>& Scene::getPrimitives()
{
	return m_primitives;
}

Primitive* Scene::getPrimitiveByID(size_t id)
{
	return m_primitives[id];
}

std::vector<Light*>& Scene::getLights()
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

int32_t Scene::getActivePrimitiveID()
{
	if (auto prim = dynamic_cast<Primitive*>(m_activeNode))
	{
		for (size_t i = 0; i < m_primitives.size(); i++)
		{
			if (m_primitives[i] == prim)
			{
				return static_cast<uint32_t>(i);
			}
		}
	}
	return -1;
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
	m_activeNode = nullptr;
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

void Scene::setActiveCamera(Camera* camera)
{
	m_activeCamera = camera;
}

Camera* Scene::getActiveCamera()
{
	return m_activeCamera;
}

void Scene::buildSceneBVH()
{
	if (m_primitives.empty())
	{
		m_sceneBVH = nullptr;
		std::cout << "No primitives in the scene to build BVH." << std::endl;
		return;
	}
	auto startTime = std::chrono::high_resolution_clock::now();

	const size_t primCount = m_primitives.size();
	m_primBboxes.resize(primCount);
	m_primCenters.resize(primCount);

	for (size_t i = 0; i < primCount; i++)
	{
		Primitive* prim = m_primitives[i];
		glm::mat4 worldMatrix = prim->getWorldMatrix();
		m_primBboxes[i] = prim->getWorldBBox(worldMatrix);
		m_primCenters[i] = m_primBboxes[i].get_center();
	}

	m_sceneBVH = std::make_unique<Bvh>(bvh::v2::DefaultBuilder<Node>::build(*m_threadPool.get(), m_primBboxes, m_primCenters));
	m_sceneBVHDirty = false;
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	std::cout << "Built scene BVH with " << m_sceneBVH->nodes.size() << " nodes for " << primCount << " primitives in " << duration << " ms." << std::endl;
}

void Scene::markSceneBVHDirty()
{
	m_sceneBVHDirty = true;
}

bool Scene::isSceneBVHDirty()
{
	return m_sceneBVHDirty;
}

void Scene::rebuildSceneBVHIfDirty()
{
	if (m_sceneBVHDirty)
	{
		buildSceneBVH();
	}
}

const Bvh* Scene::getSceneBVH() const
{
	return m_sceneBVH.get();
}
