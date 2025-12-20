#pragma once

#include <memory>
#include <type_traits>
#include <functional>
#include "transform.hpp"

class Scene;
class SceneNode;

// Snapshot that represends a data mutation in the editor.
// Taking an assumptions that every user-induced mutation of the data in the program is represented by a corresponding DataSnapshot -
// this would form a transaction stream, that could theoretically be used to restore any state of the program,
// as long as there are no breaking changes. Alternativelly, you could look at it as a stream of commands, that restore the past state.
// Practically, some modifications are not symmetrical - for example array additions and removals would invalidate pointers,
// which means that we cannot restore the data exactly. Restrain from using pointers in the data snapshots, and instead use indices/handles.
// Do not reuse the same snapshot twice - undo() operation is allowed to invalidate the snapshot.
class DataSnapshot
{
public:
	virtual ~DataSnapshot() = default;

	// Restore the state to the one prior to taking the snapshot
	virtual std::unique_ptr<DataSnapshot> undo() = 0;

	// Try to merge 2 snapshots together, this would effectivelly invalidate the passed-in snapshot.
	// Would return true if snapshots are elegible for merge and were sucessfully merged togehter.
	virtual bool merge(DataSnapshot* command) = 0;

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
class DataSnapshot_SceneNode : public DataSnapshot
{
public:
	DataSnapshot_SceneNode(Scene* inScene, SceneNode* inSceneNode);

	virtual std::unique_ptr<DataSnapshot> undo() override;
	virtual bool merge(DataSnapshot* command) override;
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
class UICommand_SceneNodeTransform : public DataSnapshot
{
public:
	UICommand_SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode);

	virtual std::unique_ptr<DataSnapshot> undo() override;
	virtual bool merge(DataSnapshot* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	// TO-DO: switch from pointers to handles
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
	Transform m_savedTransform;
};

// Snapshot that is taken prior to the node removal - 'undo()' would restore the underlying scene node from the copy.
class DataSnapshot_SceneNodeRemoval : public DataSnapshot
{
public:
	DataSnapshot_SceneNodeRemoval(
		Scene* inScene,
		SceneNode* inSceneNode);

	virtual std::unique_ptr<DataSnapshot> undo() override;
	virtual bool merge(DataSnapshot* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	// TO-DO: switch from pointers to handles
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
	std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
};

// Snapshot that respresent the state before the node was create - 'undo()' would destroy the underlying scene node.
class UICommand_AddNodeTransaction : public DataSnapshot
{
public:
	UICommand_AddNodeTransaction(
		Scene* inScene,
		SceneNode* inSceneNode);

	virtual std::unique_ptr<DataSnapshot> undo() override;
	virtual bool merge(DataSnapshot* command) override;
	virtual bool containsChanges() const override;
	virtual void onCommitTransaction() override;

protected:
	Scene* m_scene = nullptr;
	SceneNode* m_sceneNode = nullptr;
};

class UndoRedoManager
{
public:
	bool commit(std::unique_ptr<DataSnapshot>&& inTransaction);
	void undo();
	void redo();

	bool hasUndoCommands() const;
	bool hasRedoCommands() const;

	void setMergeFence();
	void clearUndoBuffer();
	void clearRedoBuffer();

private:
	std::vector<std::unique_ptr<DataSnapshot>> m_undoBuffer;
	std::vector<std::unique_ptr<DataSnapshot>> m_redoBuffer;

};

class ScopedUITransaction
{
public:
	ScopedUITransaction(UndoRedoManager* commandManager, std::unique_ptr<DataSnapshot>&& inCommand);
	~ScopedUITransaction();

	ScopedUITransaction(const ScopedUITransaction&) = delete;
	ScopedUITransaction(ScopedUITransaction&&) = delete;
	ScopedUITransaction& operator =(const ScopedUITransaction&) = delete;
	ScopedUITransaction& operator =(ScopedUITransaction&&) = delete;

private:
	std::unique_ptr<DataSnapshot> m_transaction;
	UndoRedoManager* m_commandManager;
};

template<typename TCommand>
class TScopedUITransaction : public ScopedUITransaction
{
public:
	template<typename ... TArgs>
	TScopedUITransaction(UndoRedoManager* commandManager, Scene* scene, TArgs&&... args)
		: ScopedUITransaction(commandManager, std::make_unique<TCommand>(scene, std::forward<TArgs>(args)...))
	{}
};