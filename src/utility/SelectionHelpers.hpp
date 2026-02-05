#pragma once

#include <unordered_set>

#include "commands/commandBase.hpp"

class Scene;
class SceneNode;

namespace Selection
{
	enum class DropZone {AsChild, Below };

	struct NodeDepth
	{
		SceneNode* node = nullptr;
		int depth = -1;
	};

	std::vector<NodeDepth> sortNodesByDepth(
		const std::unordered_set<SceneNode*>& selection);

	std::unique_ptr<CommandGroup> makeReparentCommands(
		Scene* scene,
		SceneNode* targetNode,
		const std::unordered_set<SceneNode*>& selection,
		DropZone dropZone);

	std::unique_ptr<CommandGroup> makeDuplicationCommands(
		Scene* scene,
		const std::unordered_set<SceneNode*>& selection);

	std::unique_ptr<CommandGroup> makeRemoveCommands(
		Scene* scene,
		const std::unordered_set<SceneNode*>& selection);
}
