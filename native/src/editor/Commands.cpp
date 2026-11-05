#include "editor/Commands.h"

#include "core/Log.h"

namespace Engine {

// ---------------------------------------------------------------- UndoStack

void UndoStack::execute(std::unique_ptr<Command> command, EditorDocument& doc) {
    command->apply(doc);
    push(std::move(command));
}

void UndoStack::pushApplied(std::unique_ptr<Command> command) {
    push(std::move(command));
}

void UndoStack::push(std::unique_ptr<Command> command) {
    // A new action after some undos: the undone commands can't be redone any
    // more (they'd apply to a scene that has since changed), so drop them.
    commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(position_), commands_.end());
    if (savedPosition_ > static_cast<long>(position_)) {
        savedPosition_ = -1; // the saved state was in the dropped part: unreachable now
    }
    commands_.push_back(std::move(command));
    ++position_;
    if (commands_.size() > Limit) {
        commands_.erase(commands_.begin());
        --position_;
        savedPosition_ = savedPosition_ > 0 ? savedPosition_ - 1 : -1;
    }
}

bool UndoStack::undo(EditorDocument& doc) {
    if (!canUndo()) return false;
    --position_;
    commands_[position_]->revert(doc);
    return true;
}

bool UndoStack::redo(EditorDocument& doc) {
    if (!canRedo()) return false;
    commands_[position_]->apply(doc);
    ++position_;
    return true;
}

std::string UndoStack::undoName() const { return canUndo() ? commands_[position_ - 1]->name() : ""; }
std::string UndoStack::redoName() const { return canRedo() ? commands_[position_]->name() : ""; }

void UndoStack::clear() {
    commands_.clear();
    position_ = 0;
    savedPosition_ = 0;
}

// ---------------------------------------------------------------- ChangeComponentCommand

ChangeComponentCommand::ChangeComponentCommand(SceneId entity, std::string key, Json before, Json after, std::string label)
    : entity_(entity), key_(std::move(key)), before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {
    if (label_.empty()) {
        label_ = before_.is_null() ? "Add " + key_ : after_.is_null() ? "Remove " + key_ : "Edit " + key_;
    }
}

void ChangeComponentCommand::set(EditorDocument& doc, const Json& value) const {
    const Entity e = doc.scene.findById(entity_);
    if (e.isNull()) {
        ENGINE_WARN("Undo: entity ", entity_, " no longer exists");
        return;
    }
    if (value.is_null()) {
        doc.serializer->removeComponent(doc.scene, e, key_);
        return;
    }
    try {
        doc.serializer->loadComponent(doc.scene, e, key_, value);
    } catch (const std::exception& error) {
        // Only possible if a command was built from bad data; say so rather than half-apply.
        ENGINE_ERROR("Could not set ", key_, " on ", doc.scene.nameOf(e), ": ", error.what());
    }
}

// ---------------------------------------------------------------- RenameCommand

void RenameCommand::set(EditorDocument& doc, const std::string& value) const {
    const Entity e = doc.scene.findById(entity_);
    if (!e.isNull()) {
        doc.scene.get<Name>(e).value = value;
    }
}

// ---------------------------------------------------------------- CreateEntitiesCommand

CreateEntitiesCommand::CreateEntitiesCommand(Json entities, SceneId parent, std::size_t index, std::string label)
    : entities_(std::move(entities)), parent_(parent), index_(index), label_(std::move(label)) {}

SceneId CreateEntitiesCommand::rootId() const {
    return entities_.empty() ? 0 : readUInt64(entities_[0], "id", 0);
}

void CreateEntitiesCommand::apply(EditorDocument& doc) {
    std::string error;
    const Entity parent = parent_ ? doc.scene.findById(parent_) : NullEntity;
    const Entity root = doc.serializer->loadEntityTree(doc.scene, entities_, true, parent, &error);
    if (root.isNull()) {
        ENGINE_ERROR(label_, " failed: ", error);
        return;
    }
    doc.scene.setSiblingIndex(root, index_);
    doc.selected = rootId();
}

void CreateEntitiesCommand::revert(EditorDocument& doc) {
    doc.scene.destroyEntity(doc.scene.findById(rootId()));
    if (doc.selected == rootId()) {
        doc.selected = 0;
    }
}

// ---------------------------------------------------------------- DeleteEntityCommand

DeleteEntityCommand::DeleteEntityCommand(const EditorDocument& doc, SceneId entity) : entity_(entity) {
    const Entity e = doc.scene.findById(entity);
    name_ = doc.scene.nameOf(e);
    entities_ = doc.serializer->saveEntityTree(doc.scene, e);
    parent_ = doc.scene.idOf(doc.scene.parentOf(e));
    index_ = doc.scene.siblingIndex(e);
}

void DeleteEntityCommand::apply(EditorDocument& doc) {
    doc.scene.destroyEntity(doc.scene.findById(entity_));
    if (doc.selected == entity_ || doc.scene.findById(doc.selected).isNull()) {
        doc.selected = 0; // the selection was this entity or one of its children
    }
}

void DeleteEntityCommand::revert(EditorDocument& doc) {
    std::string error;
    const Entity parent = parent_ ? doc.scene.findById(parent_) : NullEntity;
    const Entity root = doc.serializer->loadEntityTree(doc.scene, entities_, true, parent, &error);
    if (root.isNull()) {
        ENGINE_ERROR("Undo delete failed: ", error);
        return;
    }
    doc.scene.setSiblingIndex(root, index_);
    doc.selected = entity_;
}

// ---------------------------------------------------------------- ReparentCommand

ReparentCommand::ReparentCommand(const EditorDocument& doc, SceneId entity, SceneId newParent, std::size_t newIndex)
    : entity_(entity), newParent_(newParent), newIndex_(newIndex) {
    const Entity e = doc.scene.findById(entity);
    name_ = doc.scene.nameOf(e);
    oldParent_ = doc.scene.idOf(doc.scene.parentOf(e));
    oldIndex_ = doc.scene.siblingIndex(e);
    oldTransform_ = doc.serializer->saveComponent(doc.scene, e, "transform");
}

void ReparentCommand::apply(EditorDocument& doc) {
    const Entity e = doc.scene.findById(entity_);
    if (doc.scene.setParent(e, newParent_ ? doc.scene.findById(newParent_) : NullEntity, true)) {
        doc.scene.setSiblingIndex(e, newIndex_);
    }
}

void ReparentCommand::revert(EditorDocument& doc) {
    const Entity e = doc.scene.findById(entity_);
    doc.scene.setParent(e, oldParent_ ? doc.scene.findById(oldParent_) : NullEntity, false);
    doc.scene.setSiblingIndex(e, oldIndex_);
    // Exact old local numbers, rather than re-deriving them (which can drift slightly).
    doc.serializer->loadComponent(doc.scene, e, "transform", oldTransform_);
    doc.scene.get<Transform>(e).resetInterpolation();
}

// ---------------------------------------------------------------- helpers

std::unique_ptr<CreateEntitiesCommand> makeCreateEntityCommand(EditorDocument& doc, const std::string& name,
                                                               SceneId parent, const Json& components) {
    // The id is reserved now and stored in the command, so redo brings back
    // the same id that later commands in the history refer to.
    const SceneId id = doc.scene.reserveId();
    Json entry = {{"id", id}, {"name", name}};
    for (const auto& item : components.items()) {
        entry[item.key()] = item.value();
    }
    const Entity parentEntity = parent ? doc.scene.findById(parent) : NullEntity;
    const std::size_t index = doc.scene.childrenOf(parentEntity).size();
    return std::make_unique<CreateEntitiesCommand>(Json::array({entry}), parent, index, "Create " + name);
}

std::unique_ptr<CreateEntitiesCommand> makeDuplicateCommand(EditorDocument& doc, SceneId entity) {
    const Entity original = doc.scene.findById(entity);
    if (original.isNull()) return nullptr;
    // Load a copy with fresh ids, save it (now with those ids), and remove it
    // again: the command then holds a subtree whose ids are free and fixed,
    // so redo brings back exactly the same entities.
    const Entity parent = doc.scene.parentOf(original);
    const Entity copy = doc.serializer->loadEntityTree(doc.scene, doc.serializer->saveEntityTree(doc.scene, original),
                                                       false, parent);
    if (copy.isNull()) return nullptr;
    doc.scene.get<Name>(copy).value = doc.scene.nameOf(original) + " (copy)";
    Json entities = doc.serializer->saveEntityTree(doc.scene, copy);
    doc.scene.destroyEntity(copy);
    return std::make_unique<CreateEntitiesCommand>(std::move(entities), doc.scene.idOf(parent),
                                                   doc.scene.siblingIndex(original) + 1,
                                                   "Duplicate " + doc.scene.nameOf(original));
}

} // namespace Engine
