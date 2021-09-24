#include "cwpch.h"

#include "Crowny/Import/Importer.h"

namespace Crowny
{

    Ref<Asset> Importer::Import(const std::string& filepath, const Ref<ImportOptions>& importOptions, const Uuid& uuid)
    {
    }

    Ref<Asset> Importer::Reimport(const std::string& filepath, const Ref<ImportOptions>& importOptions)
    {
        // return Import()
    }

} // namespace Crowny