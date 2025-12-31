#include "scene.hpp"

#include <iostream>
#include <chrono>

#include "primitive.hpp"
#include "light.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "camera.hpp"

Scene::Scene(std::string_view name)
{
	this->name = name;
}

SceneNodeHandle Scene::findHandleOfNode(SceneNode* node) const
{
	if (auto prim = dynamic_cast<Primitive*>(node))
	{
		const auto it = std::ranges::find_if(
			m_primitives,
			[prim](const std::pair<SceneNodeHandle, Primitive*>& x)
			{
				return x.second == prim;
			});
		if (it != m_primitives.end())
		{
			return it->first;
		}
	}
	if (auto light = dynamic_cast<Light*>(node))
	{
		const auto it = std::ranges::find_if(
			m_lights,
			[light](const std::pair<SceneNodeHandle, Light*>& x)
			{
				return x.second == light;
			});
		if (it != m_lights.end())
		{
			return it->first;
		}
	}
	if (auto camera = dynamic_cast<Camera*>(node))
	{
		const auto it = std::ranges::find_if(
			m_cameras,
			[camera](const std::pair<SceneNodeHandle, Camera*>& x)
			{
				return x.second == camera;
			});
		if (it != m_cameras.end())
		{
			return it->first;
		}
	}
	return SceneNodeHandle::invalidHandle();
}

SceneNode* Scene::getNodeByHandle(SceneNodeHandle handle)
{
	if (auto it = m_primitives.find(handle); it != m_primitives.end())
	{
		return it->second;
	}
	if (auto it = m_lights.find(handle); it != m_lights.end())
	{
		return it->second;
	}
	if (auto it = m_cameras.find(handle); it != m_cameras.end())
	{
		return it->second;
	}
	return nullptr;
}

SceneNode* Scene::getRootNode() const
{
	return children.front().get();
}

bool Scene::isNameUsed(std::string_view name) const
{
	return m_nodeNames.contains(name);
}

uint32_t& Scene::getNameCounter(std::string_view name)
{
	auto it = m_nodeNames.find(name);
	if (it == m_nodeNames.end())
	{
		it = m_nodeNames.emplace(name, 0).first;
	}
	return it->second;
}

void Scene::addPrimitive(Primitive* primitive)
{
	validateName(primitive);
	m_primitives.emplace(SceneNodeHandle::generateHandle(), primitive);
}

void Scene::addLight(Light* light)
{
	validateName(light);
	m_lights.emplace(SceneNodeHandle::generateHandle(), light);
	setLightsDirty();
}

SceneUnorderedMap<Primitive*>& Scene::getPrimitives()
{
	return m_primitives;
}

Primitive* Scene::getPrimitiveByID(size_t id)
{
	auto it = m_primitives.find(SceneNodeHandle{static_cast<int>(id)});
	if (it != m_primitives.end())
	{
		return it->second;
	}
	return nullptr;
}

SceneUnorderedMap<Light*>& Scene::getLights()
{
	return m_lights;
}

void Scene::addCamera(Camera* camera)
{
	validateName(camera);
	m_cameras.emplace(SceneNodeHandle::generateHandle(), camera);
}

size_t Scene::getPrimitiveCount() const
{
	return m_primitives.size();
}

std::shared_ptr<Texture> Scene::getTexture(std::string_view name)
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

std::shared_ptr<Material> Scene::getMaterial(std::string_view name)
{
	auto it = m_materials.find(name);
	if (it != m_materials.end())
	{
		return it->second;
	}
	return nullptr;
}

std::vector<std::string> Scene::getMaterialNames() const
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
	if (auto activePrim = dynamic_cast<Primitive*>(m_activeNode))
	{
		for (auto& [handle, prim] : m_primitives)
		{
			if (activePrim == prim)
			{
				return static_cast<int32_t>(handle);
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
	return m_selectedNodes.contains(node);
}

void Scene::addMaterial(std::shared_ptr<Material>&& material)
{
	m_materials.emplace(material->name, material);
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
	std::unique_ptr<SceneNode> ptr;
	if (node->parent)
	{
		ptr = node->parent->removeChild(node);
	}
	if (m_activeNode == node)
	{
		m_activeNode = nullptr;
	}
	m_selectedNodes.erase(node);
	SceneNodeHandle nodeHandle = findHandleOfNode(node);
	if (dynamic_cast<Primitive*>(node))
	{
		m_primitives.erase(nodeHandle);
		markSceneBVHDirty();
	}
	if (dynamic_cast<Light*>(node))
	{
		m_lights.erase(nodeHandle);
		setLightsDirty();
	}
	if (auto camera = dynamic_cast<Camera*>(node))
	{
		if (m_cameras.size() < 1)
		{
			return;
		}
		m_cameras.erase(nodeHandle);
		if (m_activeCamera == camera)
		{
			m_activeCamera = nullptr;
		}
	}
}

SceneNode* Scene::adoptClonedNode(
	std::unique_ptr<SceneNode>&& clonedNode,
	SceneNodeHandle preferredHandle
)
{
	if (!preferredHandle.isValid())
	{
		preferredHandle = SceneNodeHandle::generateHandle();
	}

	// it's an error if a preferred handle is taken
	assert(getNodeByHandle(preferredHandle) == nullptr);
	// the parent of a cloned node must be nullptr
	assert(clonedNode->parent == nullptr);
	addChild(std::move(clonedNode));
	SceneNode* nodeClone = children.back().get();
	if (auto prim = dynamic_cast<Primitive*>(nodeClone))
	{
		m_primitives.emplace(preferredHandle, prim);
		markSceneBVHDirty();
	}
	if (auto light = dynamic_cast<Light*>(nodeClone))
	{
		m_lights.emplace(preferredHandle, light);
		setLightsDirty();
	}
	if (auto camera = dynamic_cast<Camera*>(nodeClone))
	{
		m_cameras.emplace(preferredHandle, camera);
	}
	setActiveNode(nodeClone);
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

	int32_t i = 0;
	for (auto& [handle, prim] : m_primitives)
	{
		glm::mat4 worldMatrix = prim->getWorldMatrix();
		m_primBboxes[i] = prim->getWorldBBox(worldMatrix);
		m_primCenters[i] = m_primBboxes[i].get_center();
		++i;
	}

	m_sceneBVH = std::make_unique<Bvh>(
		bvh::v2::DefaultBuilder<Node>::build(*m_threadPool.get(), m_primBboxes, m_primCenters));
	m_sceneBVHDirty = false;
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	std::cout << "Built scene BVH with " << m_sceneBVH->nodes.size() << " nodes for " << primCount << " primitives in "
		<< duration << " ms." << std::endl;
}

void Scene::markSceneBVHDirty()
{
	m_sceneBVHDirty = true;
}

bool Scene::isSceneBVHDirty() const
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
