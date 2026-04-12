#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/NodeGraphImporter.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"

namespace Crowny
{
    bool NodeGraphImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwng";
    }

    bool NodeGraphImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> NodeGraphImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Ref<NodeGraph> tempGraph;
        NodeGraphSerializer serializer(tempGraph);
        serializer.Deserialize(path);
        if (!tempGraph)
            return nullptr;
        Ref<NodeGraphAsset> asset = CreateRef<NodeGraphAsset>();
        asset->SetGraph(tempGraph);
        asset->SetName(tempGraph->GetName());
        return asset;
    }
} // namespace Crowny