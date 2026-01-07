#pragma once

#include <memory>
#include <vector>

class SnapshotBase;
class CommandBase;

// Stores undo and redo buffers of snapshots
class CommandManager
{
public:
	bool commitSnapshot(std::unique_ptr<SnapshotBase>&& snapshotBase);
	bool commitCommand(std::unique_ptr<CommandBase>&& command);
	void undo();
	void redo();

	bool hasUndoCommands() const;

	bool hasRedoCommands() const;

	void setMergeFence() const;
	void clearUndoBuffer();
	void clearRedoBuffer();

private:
	std::vector<std::unique_ptr<CommandBase>> m_undoBuffer;
	std::vector<std::unique_ptr<CommandBase>> m_redoBuffer;

	static std::unique_ptr<CommandBase> commitInternal(CommandBase* command);
	SnapshotBase* getLastUndoAsSnapshot() const;
};
