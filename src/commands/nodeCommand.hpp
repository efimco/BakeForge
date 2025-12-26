#pragma once

#include "sceneNodeHandle.hpp"
#include "commandBase.hpp"

class SceneNode;
class Scene;

namespace Command {

class DuplicateSceneNode final : public CommandBase
{
public:
    DuplicateSceneNode(
        Scene* inScene,
        SceneNode* inSceneNode,
        bool reuseNodeHandle = false);

protected:
    virtual std::unique_ptr<CommandBase> exec() override;

    Scene* m_scene = nullptr;
    SceneNodeHandle m_nodeHandle;
    std::unique_ptr<SceneNode> m_sceneNodeClone = nullptr;
    bool m_validateName = true;
};

class RemoveSceneNode final : public CommandBase
{
public:
    RemoveSceneNode(
        Scene* inScene,
        SceneNode* inSceneNode);

protected:
    virtual std::unique_ptr<CommandBase> exec() override;

    Scene* m_scene = nullptr;
    SceneNodeHandle m_nodeHandle;
};

};
