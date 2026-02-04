#pragma once

#include "sceneNodeHandle.hpp"
#include "commandBase.hpp"

class CommandGroup final : public CommandBase
{
public:
	virtual ~CommandGroup() override = default;

	void addCommand(std::unique_ptr<CommandBase>&& command);
	void reverseCommands();

	bool isEmpty() const { return m_commands.empty(); }

protected:
	// Executes this command and returns a new negation command
	virtual std::unique_ptr<CommandBase> exec() override;

	std::vector<std::unique_ptr<CommandBase>> m_commands;
};
