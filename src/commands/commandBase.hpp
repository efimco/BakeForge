#pragma once

#include <memory>

// Base class for all commands
class CommandBase
{
public:
	virtual ~CommandBase() = default;

	bool shouldBreakHistory() const
	{
		return m_breakHistory;
	}

protected:
	friend class CommandManager;

	// Executes this command and returns a new negation command
	virtual std::unique_ptr<CommandBase> exec() = 0;

	// Whether this command would erase all previous history when committed
	bool m_breakHistory : 1 = false;
};
