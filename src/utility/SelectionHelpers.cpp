#include "SelectionHelpers.hpp"

#include <algorithm>

#include "sceneNode.hpp"
#include "commands/commandGroup.hpp"
#include "commands/nodeCommand.hpp"

namespace Selection
{
	std::vector<NodeDepth> sortNodesByDepth(
		const std::unordered_set<SceneNode*>& selection)
	{
		// --- Sort by depth - this would allow us to traverse from children to parents
		std::vector<NodeDepth> nodes;

		auto FillNodeDepth = [](NodeDepth* node)
		{
			if (node->depth == -1)
			{
				int depth = 0;
				SceneNode* it = node->node;
				while (it != nullptr)
				{
					++depth;
					it = it->parent;
				}
				node->depth = depth;
			}
		};

		for (auto node : selection)
		{
			nodes.emplace_back(node);
			FillNodeDepth(&nodes.back());
		}

		std::sort(nodes.begin(), nodes.end(), [](const NodeDepth& a, const NodeDepth& b) { return a.depth > b.depth; });

		return nodes;
	}

	std::unique_ptr<CommandGroup> makeReparentCommands(
		Scene* scene,
		SceneNode* targetNode,
		const std::unordered_set<SceneNode*>& selection,
		DropZone dropZone)
	{
		std::vector<NodeDepth> nodes = sortNodesByDepth(selection);
		std::unique_ptr<CommandGroup> commandGroup = std::make_unique<CommandGroup>();
		for (auto& sortedNode : nodes)
		{
			SceneNode* draggedNode = sortedNode.node;
			if (draggedNode && draggedNode != targetNode && draggedNode->parent)
			{
				// Check we're not dropping a parent into its own child
				SceneNode* checkParent = targetNode->parent;
				bool isDescendant = false;
				while (checkParent)
				{
					if (checkParent == draggedNode)
					{
						isDescendant = true;
						break;
					}
					checkParent = checkParent->parent;
				}

				if (!isDescendant)
				{
					if (dropZone == DropZone::AsChild)
					{
						auto reparentSceneNode = std::make_unique<Command::ReparentSceneNode>(scene, draggedNode, targetNode);
						commandGroup->addCommand(std::move(reparentSceneNode));
					}
					else
					{
						SceneNode* targetParent = targetNode->parent;
						if (targetParent)
						{
							int targetIndex = targetParent->getChildIndex(targetNode);
							if (dropZone == DropZone::Below)
							{
								targetIndex++;
							}
							if (draggedNode->parent == targetParent)
							{
								int draggedIndex = targetParent->getChildIndex(draggedNode);
								if (draggedIndex < targetIndex)
								{
									targetIndex--;
								}
							}
							auto reparentSceneNode = std::make_unique<Command::ReparentSceneNode>(scene, draggedNode, targetParent, targetIndex);
							commandGroup->addCommand(std::move(reparentSceneNode));
						}
					}
				}
			}
		}
		return commandGroup;
	}

	std::unique_ptr<CommandGroup> makeDuplicationCommands(
		Scene* scene,
		const std::unordered_set<SceneNode*>& selection)
	{
		std::vector<NodeDepth> nodes = sortNodesByDepth(selection);
		std::unique_ptr<CommandGroup> commandGroup = std::make_unique<CommandGroup>();
		for (auto& sortedNode : nodes)
		{
			SceneNode* duplicatedNode = sortedNode.node;
			if (duplicatedNode)
			{
				auto createSceneNode = std::make_unique<Command::DuplicateSceneNode>(scene, duplicatedNode);
				commandGroup->addCommand(std::move(createSceneNode));
			}
		}
		return commandGroup;
	}

	std::unique_ptr<CommandGroup> makeRemoveCommands(
		Scene* scene,
		const std::unordered_set<SceneNode*>& selection)
	{
		std::vector<NodeDepth> nodes = sortNodesByDepth(selection);
		std::unique_ptr<CommandGroup> commandGroup = std::make_unique<CommandGroup>();
		for (auto& sortedNode : nodes)
		{
			SceneNode* removedNode = sortedNode.node;
			if (removedNode)
			{
				auto createSceneNode = std::make_unique<Command::RemoveSceneNode>(scene, removedNode);
				commandGroup->addCommand(std::move(createSceneNode));
			}
		}
		return commandGroup;
	}
}
