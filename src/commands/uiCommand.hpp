#pragma once

/*
*
* - SUMMARY
* This file contains classes that implement commands and data snapshots.
* 
* - Commands
* Commands represent a stored action - a set of data and logic necessary to perform an operation.
* Taking an assumptions that every user-induced mutation of the data in the program is represented by a corresponding Command -
* this would form a transaction stream, that could be used to restore any state of the program, as long as there are no breaking changes.
* Practically, some modifications are not symmetrical - for example array additions and removals would invalidate pointers,
* which means that we cannot restore the data exactly. Restrain from using pointers in the data snapshots, and instead use indices/handles.
* Do not reuse the same snapshot twice - undo() operation is allowed to invalidate the snapshot.
* 
* - Snapshots
* Unlike regular commands, data snapshots don't have specific action associated with them, instead,
* they are meant to capture a certain set of data, and restore the state if 'undo()' is called.
* 
* - Transactions
* Transactions give you an ability to encompass an operation with a scope, and execute update logic upon completion.
* 
*/

#include <algorithm>
#include <memory>
#include <type_traits>
#include <functional>
#include "transform.hpp"

class Scene;
class SceneNode;

// Base class for all commands
class Command
{
public:
	virtual ~Command() = default;

	virtual void exec() = 0;

	// Restore the state to the one prior to taking the snapshot
	// returns a command that 
	virtual std::unique_ptr<Command> undo() = 0;

	// Try to merge 2 snapshots together, this would effectivelly invalidate the passed-in snapshot.
	// Would return true if snapshots are elegible for merge and were sucessfully merged togehter.
	virtual bool merge(Command* command) = 0;

	// Whether this snapshot contains relevant changes - If a new state of data is not identical to the old.
	virtual bool containsChanges() const = 0;

	// Notify the underlying node that transaction has been completed - this represents a logical 'cutoff point',
	// after which it would make sense to actualize the state of the nodes or perform heavy operations.
	virtual void onCommitTransaction() = 0;

	void setAllowMerging(bool value) { m_allowMerging = value; }
	bool getAllowMerging() const { return m_allowMerging; }
	bool shouldBreakHistory() const { return m_breakHistory; }

protected:
	// Whether this command could be merged with identical or similar snapshots - the exact specifics are dictated by 'merge' and 'commitChanges'
	bool m_allowMerging : 1 = true;

	// Whether a commit of this command would erase all previous history
	bool m_breakHistory : 1 = false;

};

// This command would save a snapshot of the scene node with all of it's properties.
// This allows you to encompass arbitrary property changes.
// Be carefull, as it might be excessivelly expensive to do this for some node types.
class DataSnapshot_SceneNode : public Command
{
public:
	DataSnapshot_SceneNode(Scene* inScene, SceneNode* inSceneNode);

	virtual void exec() override
	{
		// DataSnapshots dont have an action associated with them
	}

	virtual std::unique_ptr<Command> undo() override;
	virtual bool merge(Command* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	// TO-DO: switch from pointers to handles -
	// this would prevent them from being invalidated,
	// which would allow you to keep 
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
	std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
};

// This command would only save a snapshot of the transform.
class DataSnapshot_SceneNodeTransform : public Command
{
public:
	DataSnapshot_SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode);

	virtual void exec() override
	{
		// DataSnapshots dont have an action associated with them
	}
	virtual std::unique_ptr<Command> undo() override;
	virtual bool merge(Command* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	// TO-DO: switch from pointers to handles
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
	Transform m_savedTransform;
};

// Snapshot that is taken prior to the node removal - 'undo()' would restore the underlying scene node from the copy.
class DataSnapshot_SceneNodeRemoval : public Command
{
public:
	DataSnapshot_SceneNodeRemoval(
		Scene* inScene,
		SceneNode* inSceneNode);

	virtual void exec() override
	{
		// DataSnapshots dont have an action associated with them
	}
	virtual std::unique_ptr<Command> undo() override;
	virtual bool merge(Command* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	// TO-DO: switch from pointers to handles
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
	std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
};

// Snapshot that respresent the state before the node was create - 'undo()' would destroy the underlying scene node.
class DataSnapshot_SceneNodeCreation : public Command
{
public:
	DataSnapshot_SceneNodeCreation(
		Scene* inScene,
		SceneNode* inSceneNode);

	virtual void exec() override
	{
		// DataSnapshots dont have an action associated with them
	}
	virtual std::unique_ptr<Command> undo() override;
	virtual bool merge(Command* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
};

// Stores undo and redo buffers of snapshots
class UndoRedoManager
{
public:
	bool commit(std::unique_ptr<Command>&& inTransaction);
	void undo();
	void redo();

	bool hasUndoCommands() const;
	bool hasRedoCommands() const;

	void setMergeFence();
	void clearUndoBuffer();
	void clearRedoBuffer();

private:
	std::vector<std::unique_ptr<Command>> m_undoBuffer;
	std::vector<std::unique_ptr<Command>> m_redoBuffer;

};

// Scoped transaction 
class ScopedTransaction
{
public:
	ScopedTransaction(UndoRedoManager* commandManager, std::unique_ptr<Command>&& inCommand);
	~ScopedTransaction();

	ScopedTransaction(const ScopedTransaction&) = delete;
	ScopedTransaction(ScopedTransaction&&) = delete;
	ScopedTransaction& operator =(const ScopedTransaction&) = delete;
	ScopedTransaction& operator =(ScopedTransaction&&) = delete;

private:
	std::unique_ptr<Command> m_transaction;
	UndoRedoManager* m_commandManager;
};

template<typename TCommand>
class TScopedTransaction : public ScopedTransaction
{
public:
	template<typename ... TArgs>
	TScopedTransaction(UndoRedoManager* commandManager, Scene* scene, TArgs&&... args)
		: ScopedTransaction(commandManager, std::make_unique<TCommand>(scene, std::forward<TArgs>(args)...))
	{}
};