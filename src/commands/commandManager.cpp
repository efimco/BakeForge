#include "commandManager.hpp"
#include "commandBase.hpp"
#include "snapshotBase.hpp"

#include <cassert>

bool CommandManager::commitSnapshot(std::unique_ptr<SnapshotBase>&& snapshotBase)
{
	if (snapshotBase->containsChanges())
	{
        // If the last entry in an undo buffer is a snapshot - attempt to merge
		if (SnapshotBase* dataSnapshot = getLastUndoAsSnapshot())
		{
			if (snapshotBase->merge(dataSnapshot))
			{
				m_undoBuffer.pop_back();
			}
		}

		snapshotBase->onCommitTransaction();
		if (snapshotBase->shouldBreakHistory())
		{
			clearUndoBuffer();
		}

		m_undoBuffer.emplace_back(std::move(snapshotBase));
		clearRedoBuffer();
	}
	return true;
}

bool CommandManager::commitCommand(std::unique_ptr<CommandBase>&& command)
{
	if (command->shouldBreakHistory())
	{
		clearUndoBuffer();
	}

	std::unique_ptr<CommandBase> commandToUndo = commitInternal(command.get());
	m_undoBuffer.emplace_back(std::move(commandToUndo));
	clearRedoBuffer();
	return true;
}

void CommandManager::undo()
{
	assert(hasUndoCommands());
	std::unique_ptr<CommandBase> commandToUndo = std::move(m_undoBuffer.back());
	m_undoBuffer.pop_back();

	std::unique_ptr<CommandBase> commandToRedo = commitInternal(commandToUndo.get());
	m_redoBuffer.emplace_back(std::move(commandToRedo));

	setMergeFence();
}

void CommandManager::redo()
{
	assert(hasRedoCommands());
	std::unique_ptr<CommandBase> commandToRedo = std::move(m_redoBuffer.back());
	m_redoBuffer.pop_back();

	std::unique_ptr<CommandBase> commandToUndo = commitInternal(commandToRedo.get());
	m_undoBuffer.emplace_back(std::move(commandToUndo));

	setMergeFence();
}

bool CommandManager::hasUndoCommands() const
{
	return !m_undoBuffer.empty();
}

bool CommandManager::hasRedoCommands() const
{
	return !m_redoBuffer.empty();
}

void CommandManager::setMergeFence()
{
	if (SnapshotBase* dataSnapshot = getLastUndoAsSnapshot())
	{
		dataSnapshot->setAllowMerging(false);
	}
}

void CommandManager::clearUndoBuffer()
{
	m_undoBuffer.clear();
}

void CommandManager::clearRedoBuffer()
{
	m_redoBuffer.clear();
}

std::unique_ptr<CommandBase> CommandManager::commitInternal(CommandBase* command)
{
    std::unique_ptr<CommandBase> commitCommand = command->exec();
	if (auto snapshot = dynamic_cast<SnapshotBase*>(command))
	{
		snapshot->onCommitTransaction();
	}
    return commitCommand;
}

SnapshotBase* CommandManager::getLastUndoAsSnapshot()
{
	return hasUndoCommands() ? dynamic_cast<SnapshotBase*>(m_undoBuffer.back().get()) : nullptr;
}
