#include "scene.hpp"

#include <consoleapi.h>
#include <iostream>

#include "baker.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "texture.hpp"

Scene::Scene(const std::string_view name)
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
	if (auto baker = dynamic_cast<Baker*>(node))
	{
		const auto it = std::ranges::find_if(
			m_bakers,
			[baker](const std::pair<SceneNodeHandle, Baker*>& x)
			{
				return x.second == baker;
			});
		if (it != m_bakers.end())
		{
			return it->first;
		}
	}
	if (auto bakerNode = dynamic_cast<BakerNode*>(node))
	{
		const auto it = std::ranges::find_if(
			m_bakerNodes,
			[bakerNode](const std::pair<SceneNodeHandle, BakerNode*>& x)
			{
				return x.second == bakerNode;
			});
		if (it != m_bakerNodes.end())
		{
			return it->first;
		}
	}
	return SceneNodeHandle::invalidHandle();
}

SceneNode* Scene::getNodeByHandle(const SceneNodeHandle handle)
{
	if (const auto it = m_primitives.find(handle); it != m_primitives.end())
	{
		return it->second;
	}
	if (const auto it = m_lights.find(handle); it != m_lights.end())
	{
		return it->second;
	}
	if (const auto it = m_cameras.find(handle); it != m_cameras.end())
	{
		return it->second;
	}
	if (const auto it = m_bakers.find(handle); it != m_bakers.end())
	{
		return it->second;
	}
	if (const auto it = m_bakerNodes.find(handle); it != m_bakerNodes.end())
	{
		return it->second;
	}
	return nullptr;
}


bool Scene::isNameUsed(const std::string_view name) const
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

void Scene::addChild(std::unique_ptr<SceneNode>&& child)
{
	if (const auto light = dynamic_cast<Light*>(child.get()))
	{
		addLight(light);
	}
	if (const auto primitive = dynamic_cast<Primitive*>(child.get()))
	{
		addPrimitive(primitive);
	}
	if (const auto camera = dynamic_cast<Camera*>(child.get()))
	{
		addCamera(camera);
	}
	if (const auto baker = dynamic_cast<Baker*>(child.get()))
	{
		addBaker(baker);
	}
	if (const auto node = dynamic_cast<BakerNode*>(child.get()))
	{
		addBakerNode(node);
	}
	SceneNode::addChild(std::move(child));
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

Light* Scene::getLightByID(const size_t id)
{
	const auto it = m_lights.find(SceneNodeHandle{static_cast<int>(id)});
	if (it != m_lights.end())
	{
		return it->second;
	}
	return nullptr;
}

SceneUnorderedMap<Primitive*>& Scene::getPrimitives()
{
	return m_primitives;
}

Primitive* Scene::getPrimitiveByID(const size_t id)
{
	const auto it = m_primitives.find(SceneNodeHandle{static_cast<int>(id)});
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

void Scene::addBaker(Baker* baker)
{
	validateName(baker);
	m_bakers.emplace(SceneNodeHandle::generateHandle(), baker);
	addBakerNode(baker->lowPoly.get());
	addBakerNode(baker->highPoly.get());
}

void Scene::addBakerNode(BakerNode* node)
{
	validateName(node);
	m_bakerNodes.emplace(SceneNodeHandle::generateHandle(), node);
}

size_t Scene::getPrimitiveCount() const
{
	return m_primitives.size();
}

std::shared_ptr<Texture> Scene::getTexture(const std::string_view name)
{
	const auto it = m_textures.find(name);
	if (it != m_textures.end())
	{
		return it->second;
	}
	return nullptr;
}

void Scene::addTexture(std::shared_ptr<Texture> texture)
{
	m_textures[texture->name] = std::move(texture);
}

std::shared_ptr<Material> Scene::getMaterial(const std::string_view name)
{
	const auto it = m_materials.find(name);
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

SceneNode* Scene::getActiveNode() const
{
	return m_activeNode;
}

int32_t Scene::getActiveNodeID() const
{
	if (m_activeNode)
	{
		return static_cast<int32_t>(findHandleOfNode(m_activeNode));
	}
	return -1;
}


void Scene::setActiveNode(SceneNode* node, const bool addToSelection)
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

bool Scene::isNodeSelected(SceneNode* node) const
{
	return m_selectedNodes.contains(node);
}

void Scene::addMaterial(std::shared_ptr<Material> material)
{
	m_materials.emplace(material->name, material);
}

bool Scene::areLightsDirty() const
{
	return m_lightsAreDirty;
}

void Scene::setLightsDirty(const bool dirty)
{
	m_lightsAreDirty = dirty;
}

void Scene::setActiveCamera(Camera* camera)
{
	m_activeCamera = camera;
}

void Scene::deleteNode(SceneNode* node)
{
	const SceneNodeHandle nodeHandle = findHandleOfNode(node);
	if (dynamic_cast<Primitive*>(node))
	{
		m_primitives.erase(nodeHandle);
	}
	if (dynamic_cast<Light*>(node))
	{
		m_lights.erase(nodeHandle);
		setLightsDirty();
	}
	if (const auto camera = dynamic_cast<Camera*>(node))
	{
		if (m_cameras.size() <= 1)
		{
			return;
		}
		m_cameras.erase(nodeHandle);
		if (m_activeCamera == camera)
		{
			m_activeCamera = nullptr;
		}
	}
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
}

SceneNode* Scene::adoptClonedNode(
	std::unique_ptr<SceneNode>&& clonedNode, SceneNodeHandle preferredHandle)
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

void Scene::setReadBackID(float readBackID)
{
	m_readBackID = readBackID;
}

float Scene::getReadBackID()
{
	return m_readBackID;
}

Camera* Scene::getActiveCamera() const
{
	return m_activeCamera;
}


void Scene::validateName(SceneNode* node)
{
	const auto genericName = node->name.substr(0, node->name.find_last_of('.'));
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

void Scene::importModel(const std::string& filepath, ComPtr<ID3D11Device> device)
{
	if (filepath.empty())
	{
		std::cout << "No file path provided for model import." << std::endl;
		return;
	}
	if (m_isImporting)
	{
		std::cout << "Already importing a model, please wait." << std::endl;
		return;
	}

	m_isImporting = true;
	m_importDevice = device;
	m_importProgress = std::make_shared<ImportProgress>();
	m_importFuture = GLTFModel::importModelAsync(filepath, device, m_importProgress);

	std::cout << "Started async import: " << filepath << std::endl;
}

void Scene::updateAsyncImport()
{
	if (m_isImporting)
	{
		if (m_importProgress)
		{
			const std::string stage = m_importProgress->getStage();
			if (!stage.empty())
			{
				std::cout << "Import progress: " << stage << std::endl;
			}
		}
		if (m_importProgress && (m_importProgress->isCompleted || m_importProgress->hasFailed))
		{
			try
			{
				auto importResult = m_importFuture.get();
				ComPtr<ID3D11DeviceContext> context;
				m_importDevice->GetImmediateContext(&context);
				GLTFModel::finalizeAsyncImport(std::move(importResult), context, this);
				std::cout << "Model import completed." << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cerr << "Model import failed: " << e.what() << std::endl;
			}
			m_isImporting = false;
			m_importProgress = nullptr;
			m_importDevice = nullptr;
		}
	}
}

std::shared_ptr<ImportProgress> Scene::getImportProgress() const
{
	return m_importProgress;
}

void Scene::saveScene(std::string_view filepath)
{
}

void Scene::loadScene(std::string_view filepath)
{
}
