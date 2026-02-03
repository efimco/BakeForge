#include "bakerNode.hpp"
#include <iostream>

#include "material.hpp"
#include "primitive.hpp"

#include "passes/bakerPass.hpp"

LowPolyNode::LowPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
}

HighPolyNode::HighPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
}

Baker::Baker(const std::string_view nodeName, ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	name = nodeName;
	movable = false;
	m_device = device;
	m_context = context;
	lowPoly = std::make_unique<LowPolyNode>("LowPolyContainer");
	highPoly = std::make_unique<HighPolyNode>("HighPolyContainer");
	textureWidth = 1024;
	cageOffset = 0.1f;
	useSmoothedNormals = 0;
}

void Baker::bake()
{
	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		std::cout << "Baking material: " << materialName << std::endl;
		bakerPass->bake(textureWidth, textureWidth, cageOffset, useSmoothedNormals);
	}
}

void Baker::requestBake()
{

	m_pendingBake = true;
}

void Baker::processPendingBake()
{
	updateState();
	if (m_pendingBake)
	{
		bake();
		m_pendingBake = false;
	}
}

std::vector<BakerPass*> Baker::getPasses()
{
	std::vector<BakerPass*> passes;
	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		passes.push_back(bakerPass.get());
	}
	return passes;
}

void Baker::updateState()
{
	for (const auto& child : lowPoly->children)
	{
		const auto prim = dynamic_cast<Primitive*>(child.get());
		if (!prim)
			continue;
		if (!prim->material)
			continue;
		if (prim->material->name.empty())
			continue;
		if (std::ranges::find_if(m_materialsToBake, [&](const std::shared_ptr<Material>& mat)
			{ return mat->name == prim->material->name; }) == m_materialsToBake.end())
		{
			m_materialsToBake.push_back(prim->material);
		}
	}

	for (const auto& material : m_materialsToBake)
	{
		std::pair<std::vector<Primitive*>, std::vector<Primitive*>> lowHighPrimsPair;

		for (const auto& child : lowPoly->children)
		{
			const auto lowPolyPrim = dynamic_cast<Primitive*>(child.get());
			if (!lowPolyPrim || !lowPolyPrim->material || lowPolyPrim->material->name != material->name)
				continue;
			lowHighPrimsPair.first.push_back(lowPolyPrim);
		}
		for (const auto& highPolyChild : highPoly->children)
		{
			const auto highPolyPrim = dynamic_cast<Primitive*>(highPolyChild.get());
			if (!highPolyPrim)
				continue;
			lowHighPrimsPair.second.push_back(highPolyPrim);
		}
		m_materialsPrimitivesMap[material->name] = std::move(lowHighPrimsPair);
	}

	for (const auto& material : m_materialsToBake)
	{
		if (m_materialsBakerPasses.contains(material->name) &&
			m_materialsPrimitivesMap[material->name] == m_materialsBakerPasses[material->name]->getPrimitivesToBake())
			continue;
		if (m_materialsPrimitivesMap[material->name].first.empty())
		{
			m_materialsBakerPasses.erase(material->name);
			continue;
		}
		if (m_materialsBakerPasses[material->name] == nullptr)
		{
			auto bakerPass = std::make_unique<BakerPass>(m_device, m_context);
			bakerPass->name = "BKR for " + material->name;
			m_materialsBakerPasses[material->name] = std::move(bakerPass);
		}
		m_materialsBakerPasses[material->name]->setPrimitivesToBake(m_materialsPrimitivesMap[material->name]);

	}
}

