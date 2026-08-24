#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"
#include "Editor/ProjectLibrary.h"
#include "Panels/NodeEditor/NodeEditorContext.h"

namespace Crowny
{
    void NodeEditorContext::SetGraph(AssetHandle<NodeGraphAsset> graphAsset)
    {
        if (m_GraphAsset)
        {
            const Path& graphPath = ProjectLibrary::Get().UuidToPath(m_GraphAsset.GetUUID());
            if (!graphPath.empty() && m_Graph)
            {
                NodeGraphSerializer serializer(m_Graph);
                serializer.Serialize(graphPath);
            }
        }

        m_GraphAsset = graphAsset;
        m_Graph = graphAsset ? graphAsset->GetGraph() : nullptr;
        m_Dirty = true;
        m_SelectedNodeID = UUID::EMPTY;
    }

} // namespace Crowny

#endif // CW_WITH_NODES
