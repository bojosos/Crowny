#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Import/ImportOptions.h"

namespace Crowny
{

    class Importer : public Module<Importer>
    {
    public:
        Ref<Asset> Import(const std::string& filepath, const Ref<ImportOptions>& importOptions, const Uuid& uuid);

        template <typename T>
        Ref<T> Import(const std::string& filepath, const Ref<ImportOptions>& importOptions, const Uuid& uuid);

        Ref<Asset> Reimport(const std::string& filepath, const Ref<ImportOptions>& importOptions);
    };

} // namespace Crowny