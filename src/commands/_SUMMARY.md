# SUMMARY
This folder contains classes that implement Undo/Redo functionality.

# Commands
Represents a stored action - a set of data and logic necessary to perform an operation.

Taking an assumptions that every user-induced mutation of the data in the program is represented by a corresponding Command -
this would form a transaction stream, that could be used to restore any state of the program, as long as there are no breaking changes.

Practically, some modifications are not symmetrical - for example array additions and removals would invalidate pointers,
which means that we cannot restore the data exactly. Restrain from using pointers in data snapshots, and instead use indices/handles.

Do not reuse the same command twice - exec() operation is allowed to invalidate the command.

# Snapshots
A special type of commands that represent a saved state for a subset of data.

Unlike regular commands, data snapshots don't have specific action associated with them, instead, 
they are meant to capture a certain set of data, and restore the state if 'exec()' is called.

# Transactions 
Transactions give you an ability to encompass an operation within a scope and execute update logic upon completion.

By combining Snapshots and Transactions you can change properties of objects without the need to introduce new commands.