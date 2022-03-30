#include "cwpch.h"

#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    class AssetManifest;

    class AssetManifestSerializer
    {
    public:
        static void Serialize(const Ref<AssetManifest>& manifest, YAML::Emitter& out);
    };

} // namespace Crowny