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
        Ref<Asset> Import(const Path& filepath, const Ref<ImportOptions>& importOptions = nullptr);

        template <typename T> Ref<T> Import(const Path& filepath, const Ref<ImportOptions>& importOptions = nullptr);

        Ref<Asset> Reimport(const Path& filepath, const Ref<ImportOptions>& importOptions);
    };

} // namespace Crowny