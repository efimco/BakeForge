#include "nodeCommand.hpp"

#include "scene.hpp"

#include <cassert>

namespace Command
{

DuplicateSceneNode::DuplicateSceneNode(Scene* inScene, SceneNode* inSceneNode, bool inValidateName)
    : m_scene(inScene)
    , m_sceneNode(inSceneNode)
    , m_validateName(inValidateName)
{
    assert(inSceneNode);
    m_breakHistory = true;
    m_sceneNodeClone = m_sceneNode->clone();
}

std::unique_ptr<CommandBase> DuplicateSceneNode::exec()
{
    if (m_validateName)
    {
        m_scene->validateName(m_sceneNodeClone.get());
    }
    SceneNode* clonedNode = m_scene->adoptClonedNode(std::move(m_sceneNodeClone));
    return std::make_unique<RemoveSceneNode>(m_scene, clonedNode);
}

RemoveSceneNode::RemoveSceneNode(Scene* inScene, SceneNode* inSceneNode)
    : m_scene(inScene)
    , m_sceneNode(inSceneNode)
{
    assert(inSceneNode);

    // Nodes in the registry are not memory-stable after recreation
    m_breakHistory = true;
}

std::unique_ptr<CommandBase> RemoveSceneNode::exec()
{
    auto createCommand = std::make_unique<DuplicateSceneNode>(m_scene, m_sceneNode, false);
    m_scene->deleteNode(m_sceneNode);
    return createCommand;
}

}