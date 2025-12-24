#pragma once

#include <memory>

#include "snapshotBase.hpp"

class Scene;
class SceneNode;
class CommandManager;

class ScopedTransaction
{
public:
	ScopedTransaction(CommandManager* commandManager, std::unique_ptr<SnapshotBase>&& inSnapshot);
	~ScopedTransaction();

	ScopedTransaction(const ScopedTransaction&) = delete;
	ScopedTransaction(ScopedTransaction&&) = delete;
	ScopedTransaction& operator =(const ScopedTransaction&) = delete;
	ScopedTransaction& operator =(ScopedTransaction&&) = delete;

private:
	std::unique_ptr<SnapshotBase> m_command;
	CommandManager* m_commandManager;
};

template<typename TCommand>
class TScopedTransaction : public ScopedTransaction
{
public:
	template<typename ... TArgs>
	TScopedTransaction(CommandManager* commandManager, Scene* scene, TArgs&&... args)
		: ScopedTransaction(commandManager, std::make_unique<TCommand>(scene, std::forward<TArgs>(args)...))
	{
	}
};