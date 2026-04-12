#pragma once

#ifdef CW_WITH_NODES

#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    class NodeGraph;

    class NodeEditorAdapter
    {
    public:
        virtual ~NodeEditorAdapter() = default;

        virtual void SyncFromGraph(const Ref<NodeGraph>& graph) = 0;
        virtual void SyncToGraph(const Ref<NodeGraph>& graph) = 0;

        virtual void Render() = 0;

        virtual void RenderAddNodeMenu(const Ref<NodeGraph>& graph) = 0;

        virtual UUID GetSelectedNodeID() const = 0;
        virtual Vector<UUID> GetSelectedNodes() const = 0;
    };

} // namespace Crowny

#endif // CW_WITH_NODES