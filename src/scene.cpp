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
	validateName(primitive);
	m_primitives.push_back(primitive);
}

void Scene::addLight(Light* light)
{
	validateName(light);
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

void Scene::deleteNode(SceneNode* node)
{
	std::unique_ptr<SceneNode>  ptr;
	if (node->parent)
	{
		ptr = node->parent->removeChild(node);
	}
	if (m_activeNode == node)
	{
		m_activeNode = nullptr;
	}
	m_selectedNodes.erase(node);
	if (auto prim = dynamic_cast<Primitive*>(node))
	{
		auto it = std::find(m_primitives.begin(), m_primitives.end(), prim);
		if (it != m_primitives.end())
		{
			m_primitives.erase(it);
		}
	}
	if (auto light = dynamic_cast<Light*>(node))
	{
		auto it = std::find(m_lights.begin(), m_lights.end(), light);
		if (it != m_lights.end())
		{
			m_lights.erase(it);
		}
	}
	if (auto camera = dynamic_cast<Camera*>(node))
	{
		if (m_cameras.size() < 1)
		{
			return;
		}
		auto it = std::find(m_cameras.begin(), m_cameras.end(), camera);
		if (it != m_cameras.end())
		{
			m_cameras.erase(it);
		}
		if (m_activeCamera == camera)
		{
			m_activeCamera = nullptr;
		}
	}
}

SceneNode* Scene::duplicateNode(SceneNode* node)
{
	SceneNode* nodeDuplicate = nullptr;
	if (node->parent)
	{
		std::unique_ptr<SceneNode> newNode = node->clone();
		validateName(newNode);
		node->parent->addChild(std::move(newNode));
		if (auto prim = dynamic_cast<Primitive*>(node))
		{
			Primitive* newPrim = dynamic_cast<Primitive*>(node->parent->children.back().get());
			m_primitives.push_back(newPrim);
			markSceneBVHDirty();
			nodeDuplicate = newPrim;
		}
		if (auto light = dynamic_cast<Light*>(node))
		{
			Light* newLight = dynamic_cast<Light*>(node->parent->children.back().get());
			m_lights.push_back(newLight);
			setLightsDirty();
			nodeDuplicate = newLight;
		}
		if (auto camera = dynamic_cast<Camera*>(node))
		{
			Camera* newCamera = dynamic_cast<Camera*>(node->parent->children.back().get());
			m_cameras.push_back(newCamera);
			nodeDuplicate = newCamera;
		}
	}
	return nodeDuplicate;
}

SceneNode* Scene::adoptClonedNode(std::unique_ptr<SceneNode>&& clonedNode)
{
	// parent of a cloned node must be nullptr
	assert(clonedNode->parent == nullptr);
	addChild(std::move(clonedNode));
	SceneNode* nodeClone = nullptr;
	if (auto prim = dynamic_cast<Primitive*>(children.back().get()))
	{
		m_primitives.push_back(prim);
		markSceneBVHDirty();
		nodeClone = prim;
	}
	if (auto light = dynamic_cast<Light*>(children.back().get()))
	{
		m_lights.push_back(light);
		setLightsDirty();
		nodeClone = light;
	}
	if (auto camera = dynamic_cast<Camera*>(children.back().get()))
	{
		m_cameras.push_back(camera);
		nodeClone = camera;
	}
	return nodeClone;
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

void Scene::validateName(SceneNode* node)
{
	auto genericName = node->name.substr(0, node->name.find_last_of('.'));
	if (isNameUsed(genericName))
	{
		uint32_t& counter = getNameCounter(genericName);
		counter++;
		node->name = genericName + "." + std::to_string(counter);
	}
	else
	{
		getNameCounter(node->name) = 0;
	}
}

const Bvh* Scene::getSceneBVH() const
{
	return m_sceneBVH.get();
}
