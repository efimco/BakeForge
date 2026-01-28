#include "baker.hpp"


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
}

void Baker::bake()
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
		std::vector<std::pair<Primitive*, Primitive*>> primitivePairs;

		for (const auto& child : lowPoly->children)
		{
			const auto prim = dynamic_cast<Primitive*>(child.get());
			if (!prim)
				continue;
			if (!prim->material || prim->material->name != material->name)
				continue;

			std::string baseName = prim->name;
			for (const auto& suffix : {"_low", "_LP", "_lo", "_Low", "_LOW"})
			{
				if (baseName.ends_with(suffix))
				{
					baseName = baseName.substr(0, baseName.length() - strlen(suffix));
					break;
				}
			}

			Primitive* matchedHighPoly = nullptr;
			for (const auto& highPolyChild : highPoly->children)
			{
				const auto highPolyPrim = dynamic_cast<Primitive*>(highPolyChild.get());
				if (!highPolyPrim)
					continue;

				std::string highPolyBaseName = highPolyPrim->name;
				for (const auto& suffix : {"_high", "_HP", "_hi", "_High", "_HIGH"})
				{
					if (highPolyBaseName.ends_with(suffix))
					{
						highPolyBaseName = highPolyBaseName.substr(0, highPolyBaseName.length() - strlen(suffix));
						break;
					}
				}

				if (highPolyBaseName == baseName)
				{
					matchedHighPoly = highPolyPrim;
					break;
				}
			}

			primitivePairs.emplace_back(prim, matchedHighPoly);
		}

		m_materialsPrimitivesMap[material->name] = std::move(primitivePairs);
	}

	for (const auto& material : m_materialsToBake)
	{
		auto bakerPass = std::make_unique<BakerPass>(m_device, m_context);
		bakerPass->name = "BKR for " + material->name;
		bakerPass->setPrimitivesToBake(m_materialsPrimitivesMap[material->name]);
		m_materialsBakerPasses[material->name] = std::move(bakerPass);
	}

	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		bakerPass->bake(textureWidth, textureWidth, cageOffset);
	}
}

void Baker::requestBake()
{

	m_pendingBake = true;
}

void Baker::processPendingBake()
{
	if (m_pendingBake)
	{
		bake();
		m_pendingBake = false;
	}
}
