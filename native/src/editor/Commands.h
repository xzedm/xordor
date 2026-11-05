#pragma once
// Undo and redo, the "command" way.
//
// The editor never changes the scene directly. Every change is a Command
// object that knows how to apply itself and how to take itself back:
//
//   UndoStack:   [ Rename ][ Move crate ][ Delete ball ] ^ [ Add light ]
//                                                       |    ^ redo goes here
//                                                     undo goes here
//
//   execute(cmd)  apply it, push it, throw away anything that could be redone
//   undo()        revert the newest applied command, step back
//   redo()        apply the next one again, step forward
//
// Commands refer to entities by SceneId, never by Entity handle: deleting an
// entity and undoing it recreates it with a *new* handle but the same
// SceneId, so older commands in the history still find it.
//
// Most commands store the data before and after as JSON, using the scene
// serializer. One command type then covers edits to any component, even ones
// added by an app the editor has never heard of.

#include "assets/SceneSerializer.h"
#include "scene/Scene.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine {

// What the editor is working on.
struct EditorDocument {
    Scene scene;
    const SceneSerializer* serializer = nullptr; // not owned
    SceneId selected = 0;                         // 0 = nothing selected
};

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(EditorDocument& doc) = 0;
    virtual void revert(EditorDocument& doc) = 0;
    // Shown in the Edit menu: "Undo Rename Crate".
    virtual std::string name() const = 0;
};

class UndoStack {
public:
    // Applies the command and records it.
    void execute(std::unique_ptr<Command> command, EditorDocument& doc);
    // Records a command whose change is already on screen. Dragging a slider
    // or a gizmo changes the scene live, every frame; when the drag ends, one
    // command holding the start and end values is pushed with this.
    void pushApplied(std::unique_ptr<Command> command);

    bool undo(EditorDocument& doc);
    bool redo(EditorDocument& doc);
    bool canUndo() const { return position_ > 0; }
    bool canRedo() const { return position_ < commands_.size(); }
    std::string undoName() const;
    std::string redoName() const;

    void clear();
    // "Saved" is a position in the history: undoing back to it makes the
    // document clean again, without comparing any data.
    void markSaved() { savedPosition_ = static_cast<long>(position_); }
    bool isDirty() const { return savedPosition_ != static_cast<long>(position_); }
    std::size_t size() const { return commands_.size(); }

    // Oldest commands are dropped beyond this. TODO(memory): count bytes, since
    // a deleted subtree can be much bigger than a rename.
    static constexpr std::size_t Limit = 500;

private:
    void push(std::unique_ptr<Command> command);

    std::vector<std::unique_ptr<Command>> commands_;
    std::size_t position_ = 0;   // commands_[0..position_) are applied
    long savedPosition_ = 0;     // -1 once the saved state was dropped from history
};

// ---------------------------------------------------------------- commands

// Sets one component to `after` (null = remove it); undo restores `before`
// (null = it didn't exist). Covers editing, adding and removing components.
class ChangeComponentCommand final : public Command {
public:
    ChangeComponentCommand(SceneId entity, std::string key, Json before, Json after, std::string label = "");
    void apply(EditorDocument& doc) override { set(doc, after_); }
    void revert(EditorDocument& doc) override { set(doc, before_); }
    std::string name() const override { return label_; }

private:
    void set(EditorDocument& doc, const Json& value) const;
    SceneId entity_;
    std::string key_;
    Json before_, after_;
    std::string label_;
};

class RenameCommand final : public Command {
public:
    RenameCommand(SceneId entity, std::string before, std::string after)
        : entity_(entity), before_(std::move(before)), after_(std::move(after)) {}
    void apply(EditorDocument& doc) override { set(doc, after_); }
    void revert(EditorDocument& doc) override { set(doc, before_); }
    std::string name() const override { return "Rename " + after_; }

private:
    void set(EditorDocument& doc, const std::string& value) const;
    SceneId entity_;
    std::string before_, after_;
};

// Brings a saved subtree (SceneSerializer::saveEntityTree) into the scene
// under `parent` at `index`; undo removes it. Used for "create", "duplicate"
// and "paste". The JSON carries fixed SceneIds, so redo recreates the same ids.
class CreateEntitiesCommand final : public Command {
public:
    CreateEntitiesCommand(Json entities, SceneId parent, std::size_t index, std::string label);
    void apply(EditorDocument& doc) override;
    void revert(EditorDocument& doc) override;
    std::string name() const override { return label_; }
    SceneId rootId() const;

private:
    Json entities_;
    SceneId parent_;
    std::size_t index_;
    std::string label_;
};

// Deletes an entity and everything below it; undo puts the whole subtree back
// with the same ids, under the same parent, at the same position.
class DeleteEntityCommand final : public Command {
public:
    // Captures the subtree now, so construct it before the delete happens.
    DeleteEntityCommand(const EditorDocument& doc, SceneId entity);
    void apply(EditorDocument& doc) override;
    void revert(EditorDocument& doc) override;
    std::string name() const override { return "Delete " + name_; }

private:
    SceneId entity_;
    std::string name_;
    Json entities_;
    SceneId parent_ = 0;
    std::size_t index_ = 0;
};

// Moves an entity to a new parent (0 = top level) and position, keeping its
// place in the world (its local transform is recalculated).
class ReparentCommand final : public Command {
public:
    ReparentCommand(const EditorDocument& doc, SceneId entity, SceneId newParent, std::size_t newIndex);
    void apply(EditorDocument& doc) override;
    void revert(EditorDocument& doc) override;
    std::string name() const override { return "Move " + name_ + " in the hierarchy"; }

private:
    SceneId entity_;
    std::string name_;
    SceneId oldParent_ = 0, newParent_ = 0;
    std::size_t oldIndex_ = 0, newIndex_ = 0;
    Json oldTransform_;
};

// Helpers used by the UI and the tests: build the commands for common actions.
// A new empty entity under `parent` (0 = top level). Returns the command; its
// rootId() is the new entity's id.
std::unique_ptr<CreateEntitiesCommand> makeCreateEntityCommand(EditorDocument& doc, const std::string& name,
                                                               SceneId parent, const Json& components = Json::object());
// A copy of an entity and its children, with fresh ids, next to the original.
std::unique_ptr<CreateEntitiesCommand> makeDuplicateCommand(EditorDocument& doc, SceneId entity);

} // namespace Engine
