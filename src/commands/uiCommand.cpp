#include "commands/uiCommand.hpp"

#include "scene.hpp"
#include "sceneNode.hpp"

DataSnapshot_SceneNode::DataSnapshot_SceneNode(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_sceneNodeClone = m_sceneNode->clone();
}

std::unique_ptr<Command> DataSnapshot_SceneNode::undo()
{
	auto redoTransaction = std::make_unique<DataSnapshot_SceneNode>(m_scene, m_sceneNode);
	m_sceneNode->copyFrom(m_sceneNodeClone.get());
	return redoTransaction;
}

bool DataSnapshot_SceneNode::merge(Command* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (auto* castCommand = dynamic_cast<DataSnapshot_SceneNode*>(command))
		{
			if (castCommand->m_sceneNode == m_sceneNode)
			{
				m_sceneNodeClone = std::move(castCommand->m_sceneNodeClone);
				castCommand->m_sceneNodeClone = nullptr;
				return true;
			}
		}
	}
	return false;
}

bool DataSnapshot_SceneNode::containsChanges() const
{
	return m_sceneNodeClone->differsFrom(m_sceneNode);
}

void DataSnapshot_SceneNode::onCommitTransaction()
{
	m_sceneNode->onCommitTransaction(m_scene);
}

DataSnapshot_SceneNodeTransform::DataSnapshot_SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	m_savedTransform = m_sceneNode->transform;
}

std::unique_ptr<Command> DataSnapshot_SceneNodeTransform::undo()
{
	auto redoTransaction = std::make_unique<DataSnapshot_SceneNodeTransform>(m_scene, m_sceneNode);
	m_sceneNode->transform = m_savedTransform;
	return redoTransaction;
}

bool DataSnapshot_SceneNodeTransform::merge(Command* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (auto* castCommand = dynamic_cast<DataSnapshot_SceneNodeTransform*>(command))
		{
			if (castCommand->m_sceneNode == m_sceneNode)
			{
				m_savedTransform = castCommand->m_savedTransform;
				return true;
			}
		}
	}
	return false;
}

bool DataSnapshot_SceneNodeTransform::containsChanges() const
{
	return
		m_sceneNode->transform.position != m_savedTransform.position ||
		m_sceneNode->transform.rotation != m_savedTransform.rotation ||
		m_sceneNode->transform.scale != m_savedTransform.scale;
}

void DataSnapshot_SceneNodeTransform::onCommitTransaction()
{
	m_sceneNode->onCommitTransaction(m_scene);
}

bool UndoRedoManager::commit(std::unique_ptr<Command>&& inTransaction)
{
	bool containsChanges = inTransaction->containsChanges();
	if (containsChanges)
	{
		if (hasUndoCommands() && inTransaction->merge(m_undoBuffer.back().get()))
		{
			m_undoBuffer.pop_back();
		}
		inTransaction->onCommitTransaction();

		if (inTransaction->shouldBreakHistory())
		{
			clearUndoBuffer();
		}

		m_undoBuffer.emplace_back(inTransaction.release());
		clearRedoBuffer();
	}
	return true;
}

DataSnapshot_SceneNodeRemoval::DataSnapshot_SceneNodeRemoval(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_sceneNodeClone = m_sceneNode->clone();

	// Nodes in the registry are not memory-stable after recreation
	m_breakHistory = true;
}

std::unique_ptr<Command> DataSnapshot_SceneNodeRemoval::undo()
{
	SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone));
	return std::make_unique<DataSnapshot_SceneNodeCreation>(m_scene, clonedNode);
}

bool DataSnapshot_SceneNodeRemoval::merge(Command* command)
{
	return false;
}

bool DataSnapshot_SceneNodeRemoval::containsChanges() const
{
	return true;
}

void DataSnapshot_SceneNodeRemoval::onCommitTransaction()
{
}

DataSnapshot_SceneNodeCreation::DataSnapshot_SceneNodeCreation(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_breakHistory = true;
}

std::unique_ptr<Command> DataSnapshot_SceneNodeCreation::undo()
{
	auto redoTransaction = std::make_unique<DataSnapshot_SceneNode>(m_scene, m_sceneNode);
	m_scene->deleteNode(m_sceneNode);
	return redoTransaction;
}

bool DataSnapshot_SceneNodeCreation::merge(Command* command)
{
	return false;
}

bool DataSnapshot_SceneNodeCreation::containsChanges() const
{
	return true;
}

void DataSnapshot_SceneNodeCreation::onCommitTransaction()
{
}

void UndoRedoManager::undo()
{
	assert(hasUndoCommands());
	std::unique_ptr<Command> commandToUndo = std::move(m_undoBuffer.back());
	m_undoBuffer.pop_back();
	m_redoBuffer.emplace_back(commandToUndo->undo());
	m_redoBuffer.back()->setAllowMerging(false);
	commandToUndo->onCommitTransaction();
}

void UndoRedoManager::redo()
{
	assert(hasRedoCommands());
	std::unique_ptr<Command> commandToRedo = std::move(m_redoBuffer.back());
	m_redoBuffer.pop_back();
	m_undoBuffer.emplace_back(commandToRedo->undo());
	m_undoBuffer.back()->setAllowMerging(false);
	commandToRedo->onCommitTransaction();
}

bool UndoRedoManager::hasUndoCommands() const
{
	return !m_undoBuffer.empty();
}

bool UndoRedoManager::hasRedoCommands() const
{
	return !m_redoBuffer.empty();
}

void UndoRedoManager::setMergeFence()
{
	if (hasUndoCommands())
	{
		std::unique_ptr<Command>& lastTransaction = m_undoBuffer.back();
		lastTransaction->setAllowMerging(false);
	}
}

void UndoRedoManager::clearUndoBuffer()
{
	m_undoBuffer.clear();
}

void UndoRedoManager::clearRedoBuffer()
{
	m_redoBuffer.clear();
}

ScopedTransaction::ScopedTransaction(UndoRedoManager* commandManager, std::unique_ptr<Command>&& inCommand)
	: m_commandManager(commandManager)
	, m_transaction(std::move(inCommand))
{
}

ScopedTransaction::~ScopedTransaction()
{
	m_commandManager->commit(std::move(m_transaction));
}
