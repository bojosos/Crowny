#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    namespace
    {
        struct PlannedTransform
        {
            Entity Target;
            Entity Parent;
            glm::vec3 Position{ 0.0f };
            glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 Scale{ 1.0f };
        };

        struct SubtreeEntry
        {
            Entity Value;
            Entity ExpectedParent;
            bool Expanded = false;
        };

        bool AddUniqueEntity(Vector<Entity>& entities, UnorderedSet<entt::entity>& handles, Entity entity)
        {
            if (!entity || !handles.insert(entity.GetHandle()).second)
                return false;
            entities.push_back(entity);
            return true;
        }

        bool NormalizeRoots(Scene& scene, std::span<const Entity> input, Vector<Entity>& roots, HierarchyMutationStats& stats,
                            uint32_t* uniqueCandidateCount = nullptr)
        {
            stats.InputEntityCount = static_cast<uint32_t>(input.size());
            Vector<Entity> candidates;
            candidates.reserve(input.size());
            UnorderedSet<entt::entity> candidateHandles;
            candidateHandles.reserve(input.size());
            const Entity sceneRoot = scene.GetRootEntity();

            for (Entity entity : input)
            {
                if (!entity || entity.GetScene() != &scene || (sceneRoot && entity == sceneRoot))
                    return false;
                AddUniqueEntity(candidates, candidateHandles, entity);
            }
            if (uniqueCandidateCount != nullptr)
                *uniqueCandidateCount = static_cast<uint32_t>(candidates.size());

            roots.reserve(candidates.size());
            UnorderedSet<entt::entity> ancestorPath;
            for (Entity entity : candidates)
            {
                bool hasSelectedAncestor = false;
                ancestorPath.clear();
                for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
                {
                    if (parent.GetScene() != &scene || !ancestorPath.insert(parent.GetHandle()).second)
                        return false;
                    if (candidateHandles.find(parent.GetHandle()) != candidateHandles.end())
                    {
                        hasSelectedAncestor = true;
                        break;
                    }
                }
                if (!hasSelectedAncestor)
                    roots.push_back(entity);
            }
            stats.RootEntityCount = static_cast<uint32_t>(roots.size());
            return true;
        }

        bool PlanWorldPreservingTransform(Entity entity, Entity newParent, PlannedTransform& planned)
        {
            glm::mat4 localMatrix = entity.GetWorldMatrix();
            if (newParent)
            {
                const glm::mat4& parentWorld = newParent.GetWorldMatrix();
                if (std::abs(glm::determinant(glm::mat3(parentWorld))) <= std::numeric_limits<float>::epsilon())
                    return false;
                localMatrix = glm::inverse(parentWorld) * localMatrix;
            }

            planned.Target = entity;
            planned.Parent = newParent;
            if (!Math::DecomposeMatrix(localMatrix, planned.Position, planned.Rotation, planned.Scale))
                return false;
            planned.Rotation = glm::normalize(planned.Rotation);
            return true;
        }

        void ApplyPlannedTransform(PlannedTransform& planned)
        {
            planned.Target.GetTransform().SetLocalTransform(Transform(planned.Position, planned.Rotation, planned.Scale));
        }

        void CommitParentChildren(Entity parent, Vector<Entity>& rebuilt, HierarchyMutationStats& stats)
        {
            auto& current = parent.GetComponent<RelationshipComponent>().Children;
            size_t firstChanged = 0u;
            const size_t commonSize = std::min(current.size(), rebuilt.size());
            while (firstChanged < commonSize && current[firstChanged] == rebuilt[firstChanged])
                firstChanged++;
            if (firstChanged == current.size() && firstChanged == rebuilt.size())
                return;

            current.swap(rebuilt);
            stats.ParentVectorRebuildCount++;
            for (size_t index = firstChanged; index < current.size(); index++)
            {
                current[index].GetComponent<RelationshipComponent>().SiblingIndex = static_cast<uint32_t>(index);
                stats.SiblingIndexWriteCount++;
            }
        }

        void CollectParent(Vector<Entity>& parents, UnorderedSet<entt::entity>& parentHandles, Entity parent)
        {
            if (parent)
                AddUniqueEntity(parents, parentHandles, parent);
        }

        bool ValidateParentReferences(Scene& scene, std::span<const Entity> entities)
        {
            UnorderedSet<entt::entity> entityHandles;
            entityHandles.reserve(entities.size());
            Vector<Entity> parents;
            UnorderedSet<entt::entity> parentHandles;
            parentHandles.reserve(entities.size());
            for (Entity entity : entities)
            {
                entityHandles.insert(entity.GetHandle());
                CollectParent(parents, parentHandles, entity.GetParent());
            }

            UnorderedSet<entt::entity> referencedHandles;
            referencedHandles.reserve(entities.size());
            for (Entity parent : parents)
            {
                if (parent.GetScene() != &scene)
                    return false;
                for (Entity child : parent.GetChildren())
                {
                    if (!child || entityHandles.find(child.GetHandle()) == entityHandles.end())
                        continue;
                    if (child.GetScene() != &scene || child.GetParent() != parent ||
                        !referencedHandles.insert(child.GetHandle()).second)
                        return false;
                }
            }

            for (Entity entity : entities)
            {
                if (entity.GetParent() && referencedHandles.find(entity.GetHandle()) == referencedHandles.end())
                    return false;
            }
            return true;
        }

        Vector<Entity> BuildChildrenWithout(const Vector<Entity>& current, const UnorderedSet<entt::entity>& removed)
        {
            Vector<Entity> rebuilt;
            rebuilt.reserve(current.size());
            for (Entity child : current)
            {
                if (child && removed.find(child.GetHandle()) == removed.end())
                    rebuilt.push_back(child);
            }
            return rebuilt;
        }

        bool CollectSubtrees(Scene& scene, std::span<const Entity> roots, Vector<Entity>& postOrder,
                             UnorderedSet<entt::entity>& collected)
        {
            Vector<SubtreeEntry> stack;
            stack.reserve(roots.size());
            for (auto root = roots.rbegin(); root != roots.rend(); ++root)
                stack.push_back({ *root, {}, false });

            while (!stack.empty())
            {
                const SubtreeEntry entry = stack.back();
                stack.pop_back();
                if (!entry.Value || entry.Value.GetScene() != &scene ||
                    (entry.ExpectedParent && entry.Value.GetParent() != entry.ExpectedParent))
                    return false;
                if (entry.Expanded)
                {
                    postOrder.push_back(entry.Value);
                    continue;
                }
                if (!collected.insert(entry.Value.GetHandle()).second)
                    return false;

                stack.push_back({ entry.Value, entry.ExpectedParent, true });
                const Vector<Entity>& children = entry.Value.GetChildren();
                for (auto child = children.rbegin(); child != children.rend(); ++child)
                    stack.push_back({ *child, entry.Value, false });
            }
            return true;
        }
    } // namespace

    HierarchyMutationResult Scene::ReparentEntities(std::span<const Entity> entities, Entity newParent, uint32_t insertionIndex)
    {
        HierarchyMutationResult result;
        if (newParent && (!newParent.IsValid() || newParent.GetScene() != this))
            return result;

        Vector<Entity> roots;
        if (!NormalizeRoots(*this, entities, roots, result.Stats))
            return result;
        if (roots.empty())
        {
            result.Succeeded = true;
            return result;
        }
        if (!ValidateParentReferences(*this, roots))
            return result;

        Vector<Entity> moving;
        moving.reserve(roots.size());
        for (Entity entity : roots)
        {
            if (newParent && (newParent == entity || newParent.IsChildOf(entity)))
                return result;
            if (insertionIndex == std::numeric_limits<uint32_t>::max() && entity.GetParent() == newParent)
                continue;
            moving.push_back(entity);
        }
        if (moving.empty())
        {
            result.Succeeded = true;
            return result;
        }

        Vector<PlannedTransform> transforms;
        transforms.reserve(moving.size());
        for (Entity entity : moving)
        {
            if (entity.GetParent() == newParent)
                continue;
            PlannedTransform planned;
            if (!PlanWorldPreservingTransform(entity, newParent, planned))
                return result;
            transforms.push_back(planned);
        }

        UnorderedSet<entt::entity> movingHandles;
        movingHandles.reserve(moving.size());
        Vector<Entity> touchedParents;
        UnorderedSet<entt::entity> touchedParentHandles;
        touchedParentHandles.reserve(moving.size() + 1u);
        for (Entity entity : moving)
        {
            movingHandles.insert(entity.GetHandle());
            CollectParent(touchedParents, touchedParentHandles, entity.GetParent());
        }
        CollectParent(touchedParents, touchedParentHandles, newParent);

        auto transformScope = DeferTransformChanges();

        for (Entity parent : touchedParents)
        {
            const Vector<Entity>& current = parent.GetChildren();
            Vector<Entity> rebuilt = BuildChildrenWithout(current, movingHandles);
            if (parent == newParent)
            {
                const size_t insertAt = insertionIndex == std::numeric_limits<uint32_t>::max()
                                          ? rebuilt.size()
                                          : std::min<size_t>(insertionIndex, rebuilt.size());
                rebuilt.insert(rebuilt.begin() + insertAt, moving.begin(), moving.end());
            }
            CommitParentChildren(parent, rebuilt, result.Stats);
        }

        for (Entity entity : moving)
        {
            auto& relationship = entity.GetComponent<RelationshipComponent>();
            relationship.Parent = newParent;
            if (!newParent)
                relationship.SiblingIndex = 0u;
        }
        for (PlannedTransform& planned : transforms)
            ApplyPlannedTransform(planned);
        MarkHierarchyTopologyDirty();
        for (PlannedTransform& planned : transforms)
        {
            planned.Target.NotifyTransformChanged();
            result.Stats.TransformInvalidationRootCount++;
        }

        result.Stats.ReparentedEntityCount = static_cast<uint32_t>(moving.size());
        result.Succeeded = true;
        return result;
    }

    HierarchyMutationResult Scene::DestroyEntities(std::span<const Entity> entities, HierarchyDestroyMode mode)
    {
        HierarchyMutationResult result;
        Vector<Entity> roots;
        uint32_t uniqueCandidateCount = 0u;
        if (!NormalizeRoots(*this, entities, roots, result.Stats, &uniqueCandidateCount))
            return result;
        if (mode == HierarchyDestroyMode::PreserveChildren && roots.size() != uniqueCandidateCount)
            return result;
        if (roots.empty())
        {
            result.Succeeded = true;
            return result;
        }
        if (!ValidateParentReferences(*this, roots))
            return result;

        if (mode == HierarchyDestroyMode::PreserveChildren)
        {
            UnorderedSet<entt::entity> rootHandles;
            rootHandles.reserve(roots.size());
            UnorderedSet<entt::entity> preservedChildHandles;
            Vector<PlannedTransform> transforms;
            size_t preservedChildCount = 0u;
            for (Entity root : roots)
                preservedChildCount += root.GetChildCount();
            transforms.reserve(preservedChildCount);
            preservedChildHandles.reserve(preservedChildCount);
            for (Entity root : roots)
            {
                rootHandles.insert(root.GetHandle());
                const Entity parent = root.GetParent();
                for (Entity child : root.GetChildren())
                {
                    if (!child || child.GetScene() != this || child.GetParent() != root ||
                        !preservedChildHandles.insert(child.GetHandle()).second)
                        return result;
                    PlannedTransform planned;
                    if (!PlanWorldPreservingTransform(child, parent, planned))
                        return result;
                    transforms.push_back(planned);
                }
            }

            auto transformScope = DeferTransformChanges();

            Vector<Entity> touchedParents;
            UnorderedSet<entt::entity> touchedParentHandles;
            touchedParentHandles.reserve(roots.size());
            for (Entity root : roots)
                CollectParent(touchedParents, touchedParentHandles, root.GetParent());

            for (Entity parent : touchedParents)
            {
                const Vector<Entity>& current = parent.GetChildren();
                Vector<Entity> rebuilt;
                size_t finalSize = current.size();
                for (Entity child : current)
                {
                    if (rootHandles.find(child.GetHandle()) != rootHandles.end())
                    {
                        finalSize--;
                        finalSize += child.GetChildCount();
                    }
                }
                rebuilt.reserve(finalSize);
                for (Entity child : current)
                {
                    if (rootHandles.find(child.GetHandle()) == rootHandles.end())
                    {
                        rebuilt.push_back(child);
                        continue;
                    }
                    for (Entity grandChild : child.GetChildren())
                        rebuilt.push_back(grandChild);
                }
                CommitParentChildren(parent, rebuilt, result.Stats);
            }

            for (PlannedTransform& planned : transforms)
            {
                auto& relationship = planned.Target.GetComponent<RelationshipComponent>();
                relationship.Parent = planned.Parent;
                if (!planned.Parent)
                    relationship.SiblingIndex = 0u;
                ApplyPlannedTransform(planned);
            }

            for (Entity root : roots)
                root.GetComponent<RelationshipComponent>().Children.clear();

            for (Entity root : roots)
            {
                m_EntityMap.erase(root.GetUuid());
                m_Registry.destroy(root.GetHandle());
                result.Stats.DestroyedEntityCount++;
            }
            MarkHierarchyTopologyDirty();
            for (PlannedTransform& planned : transforms)
            {
                planned.Target.NotifyTransformChanged();
                result.Stats.TransformInvalidationRootCount++;
            }
            result.Stats.ReparentedEntityCount = static_cast<uint32_t>(transforms.size());
            result.Succeeded = true;
            return result;
        }

        Vector<Entity> postOrder;
        UnorderedSet<entt::entity> destroyedHandles;
        if (!CollectSubtrees(*this, roots, postOrder, destroyedHandles))
            return result;

        Vector<Entity> touchedParents;
        UnorderedSet<entt::entity> touchedParentHandles;
        touchedParentHandles.reserve(roots.size());
        for (Entity root : roots)
        {
            Entity parent = root.GetParent();
            if (parent && destroyedHandles.find(parent.GetHandle()) == destroyedHandles.end())
                CollectParent(touchedParents, touchedParentHandles, parent);
        }
        for (Entity parent : touchedParents)
        {
            Vector<Entity> rebuilt = BuildChildrenWithout(parent.GetChildren(), destroyedHandles);
            CommitParentChildren(parent, rebuilt, result.Stats);
        }

        for (Entity entity : postOrder)
            entity.GetComponent<RelationshipComponent>().Children.clear();

        for (Entity entity : postOrder)
        {
            m_EntityMap.erase(entity.GetUuid());
            m_Registry.destroy(entity.GetHandle());
            result.Stats.DestroyedEntityCount++;
        }
        MarkHierarchyTopologyDirty();
        result.Succeeded = true;
        return result;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        const Array<Entity, 1> entities{ entity };
        DestroyEntities(entities);
    }
} // namespace Crowny
