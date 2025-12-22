#pragma once

#include "commandBase.hpp"

class SnapshotBase : public CommandBase
{
public:

    void setAllowMerging(bool value) { m_allowMerging = value; }

    [[nodiscard]]
    bool getAllowMerging() const { return m_allowMerging; }

protected:
    friend class CommandManager;

    // Try to merge 2 snapshots together, this would effectively invalidate the passed-in snapshot.
    // Would return true if snapshots are eligible for merge and were successfully merged together.
    virtual bool merge(SnapshotBase* command) = 0;

    // Whether this snapshot contains relevant changes - If a new state of data is not identical to the old.
    [[nodiscard]]
    virtual bool containsChanges() const = 0;

    // Notify the underlying node that transaction has been completed - this represents a logical 'cutoff point',
    // after which it would make sense to actualize the state of the nodes or perform heavy operations.
    virtual void onCommitTransaction() = 0;

    // Whether this command could be merged with identical or similar snapshots - the exact specifics are dictated by 'merge' and 'commitChanges'
    bool m_allowMerging : 1 = true;

};
