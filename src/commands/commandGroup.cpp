#include "commandGroup.hpp"

#include <cassert>
#include <algorithm>

void CommandGroup::addCommand(std::unique_ptr<CommandBase>&& command)
{
	m_commands.emplace_back(std::move(command));
}

void CommandGroup::reverseCommands()
{
	std::ranges::reverse(m_commands);
}

std::unique_ptr<CommandBase> CommandGroup::exec()
{
	int commandNumber = 0;
	std::unique_ptr<CommandGroup> undoCommandGroup = std::make_unique<CommandGroup>();
	for (std::unique_ptr<CommandBase>& commandBase : m_commands)
	{
		commandBase->setNumberInGroup(commandNumber);
		undoCommandGroup->addCommand(commandBase->exec());
		commandNumber++;
	}
	undoCommandGroup->reverseCommands();
	return undoCommandGroup;
}
