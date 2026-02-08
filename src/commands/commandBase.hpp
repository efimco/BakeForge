#pragma once

#include <memory>

// Base class for all commands
class CommandBase
{
public:
	static constexpr int k_invalidNumberInGroup = -1;

	virtual ~CommandBase() = default;

	bool shouldBreakHistory() const
	{
		return m_breakHistory;
	}

	void setNumberInGroup(int number) { m_numberInGroup = number; }
	int getNumberInGroup() const { return m_numberInGroup; }

protected:
	friend class CommandManager;
	friend class CommandGroup;

	// Executes this command and returns a new negation command
	virtual std::unique_ptr<CommandBase> exec() = 0;

	int m_numberInGroup = k_invalidNumberInGroup;

	// Whether this command would erase all previous history when committed
	bool m_breakHistory : 1 = false;
};
