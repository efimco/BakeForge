#include "scopedTransaction.hpp"
#include "commandManager.hpp"

ScopedTransaction::ScopedTransaction(CommandManager* commandManager, std::unique_ptr<SnapshotBase>&& inSnapshot)
    : m_command(std::move(inSnapshot))
    , m_commandManager(commandManager)
{
}

ScopedTransaction::~ScopedTransaction()
{
    m_commandManager->commitSnapshot(std::move(m_command));
}
