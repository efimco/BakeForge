#include "baker.hpp"
#include "passes/bakerPass.hpp"

#include "material.hpp"
#include "primitive.hpp"


LowPolyNode::LowPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
	cageOffset = 0.01f;
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
}

void Baker::bake()
{
	std::vector<std::shared_ptr<Material>> materialsToBake;
	for (const auto& child : lowPoly->children)
	{
		const auto prim = dynamic_cast<Primitive*>(child.get());
		if (!prim)
			continue;
		if (!prim->material)
			continue;
		if (prim->material->name.empty())
			continue;
		if (std::ranges::find_if(materialsToBake,
			[&](const std::shared_ptr<Material>& mat)
			{
				return mat->name == prim->material->name;
			}) == materialsToBake.end())
		{
			materialsToBake.push_back(prim->material);
		}
	}

	std::unordered_map<std::string, std::pair<std::vector<Primitive*>, std::vector<Primitive*>> > materialsPrimitivesMap;
	for (const auto& material : materialsToBake)
	{
		std::vector<Primitive*> lowPolyPrimitives;
		std::vector<Primitive*> highPolyPrimitives;

		for (const auto& child : lowPoly->children)
		{
			if (const auto prim = dynamic_cast<Primitive*>(child.get()))
			{
				if (prim->material->name == material->name)
				{
					lowPolyPrimitives.push_back(prim);
				}
			}
		}
		for (const auto& child : highPoly->children)
		{
			if (const auto prim = dynamic_cast<Primitive*>(child.get()))
			{
				if (prim->material->name == material->name)
				{
					highPolyPrimitives.push_back(prim);
				}
			}
		}
		std::pair<std::vector<Primitive*>, std::vector<Primitive*> > pair = { lowPolyPrimitives, highPolyPrimitives };
		materialsPrimitivesMap[material->name] = std::move(pair);
	}


	std::vector<std::unique_ptr<BakerPass>> materialsBakerPasses;
	for (const auto& material : materialsToBake)
	{
		// For each material, create a BakerPass and bake the textures
		auto bakerPass = std::make_unique<BakerPass>(m_device, m_context);
		bakerPass->name = "Baker Pass for " + name;
		// Here you would set up the bakerPass with the material properties
		// and call the bake function. This is a placeholder for demonstration.
		// bakerPass.bakeMaterial(material, textureWidth, textureWidth);
		materialsBakerPasses.push_back(std::move(bakerPass));
	}
}
