#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Panels/NodeEditor/NodeEditorContext.h"
#include "Editor/ProjectLibrary.h"
#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{
    void NodeEditorContext::SetGraph(AssetHandle<NodeGraphAsset> graphAsset)
    {
        if (m_GraphAsset)
        {
            const Path& graphPath = ProjectLibrary::Get().UuidToPath(m_GraphAsset.GetUUID());
            if (!graphPath.empty())
                AssetManager::Get().Save(m_GraphAsset, graphPath, true);
        }

        m_GraphAsset = graphAsset;
        m_Graph = graphAsset->GetGraph();
        m_Dirty = true;
        m_SelectedNodeID = UUID::EMPTY;
    }

} // namespace Crowny

#endif // CW_WITH_NODES
