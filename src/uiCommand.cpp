#include "uiCommand.hpp"

#include "scene.hpp"
#include "sceneNode.hpp"

DataSnapshot_SceneNode::DataSnapshot_SceneNode(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_sceneNodeClone = m_sceneNode->clone();
}

std::unique_ptr<DataSnapshot> DataSnapshot_SceneNode::undo()
{
	auto redoTransaction = std::make_unique<DataSnapshot_SceneNode>(m_scene, m_sceneNode);
	m_sceneNode->copyFrom(m_sceneNodeClone.get());
	return redoTransaction;
}

bool DataSnapshot_SceneNode::merge(DataSnapshot* command)
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

UICommand_SceneNodeTransform::UICommand_SceneNodeTransform(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	m_savedTransform = m_sceneNode->transform;
}

std::unique_ptr<DataSnapshot> UICommand_SceneNodeTransform::undo()
{
	auto redoTransaction = std::make_unique<UICommand_SceneNodeTransform>(m_scene, m_sceneNode);
	m_sceneNode->transform = m_savedTransform;
	return redoTransaction;
}

bool UICommand_SceneNodeTransform::merge(DataSnapshot* command)
{
	if (getAllowMerging() && command->getAllowMerging())
	{
		if (auto* castCommand = dynamic_cast<UICommand_SceneNodeTransform*>(command))
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

bool UICommand_SceneNodeTransform::containsChanges() const
{
	return
		m_sceneNode->transform.position != m_savedTransform.position ||
		m_sceneNode->transform.rotation != m_savedTransform.rotation ||
		m_sceneNode->transform.scale != m_savedTransform.scale;
}

void UICommand_SceneNodeTransform::onCommitTransaction()
{
	m_sceneNode->onCommitTransaction(m_scene);
}

bool UndoRedoManager::commit(std::unique_ptr<DataSnapshot>&& inTransaction)
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

std::unique_ptr<DataSnapshot> DataSnapshot_SceneNodeRemoval::undo()
{
	SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone));
	return std::make_unique<UICommand_AddNodeTransaction>(m_scene, clonedNode);
}

bool DataSnapshot_SceneNodeRemoval::merge(DataSnapshot* command)
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

UICommand_AddNodeTransaction::UICommand_AddNodeTransaction(Scene* inScene, SceneNode* inSceneNode)
	: m_scene(inScene)
	, m_sceneNode(inSceneNode)
{
	assert(inSceneNode);
	m_breakHistory = true;
}

std::unique_ptr<DataSnapshot> UICommand_AddNodeTransaction::undo()
{
	auto redoTransaction = std::make_unique<DataSnapshot_SceneNode>(m_scene, m_sceneNode);
	m_scene->deleteNode(m_sceneNode);
	return redoTransaction;
}

bool UICommand_AddNodeTransaction::merge(DataSnapshot* command)
{
	return false;
}

bool UICommand_AddNodeTransaction::containsChanges() const
{
	return true;
}

void UICommand_AddNodeTransaction::onCommitTransaction()
{
}

void UndoRedoManager::undo()
{
	assert(hasUndoCommands());
	std::unique_ptr<DataSnapshot> commandToUndo = std::move(m_undoBuffer.back());
	m_undoBuffer.pop_back();
	m_redoBuffer.emplace_back(commandToUndo->undo());
	m_redoBuffer.back()->setAllowMerging(false);
	commandToUndo->onCommitTransaction();
}

void UndoRedoManager::redo()
{
	assert(hasRedoCommands());
	std::unique_ptr<DataSnapshot> commandToRedo = std::move(m_redoBuffer.back());
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
		std::unique_ptr<DataSnapshot>& lastTransaction = m_undoBuffer.back();
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

ScopedUITransaction::ScopedUITransaction(UndoRedoManager* commandManager, std::unique_ptr<DataSnapshot>&& inCommand)
	: m_commandManager(commandManager)
	, m_transaction(std::move(inCommand))
{
}

ScopedUITransaction::~ScopedUITransaction()
{
	m_commandManager->commit(std::move(m_transaction));
}
