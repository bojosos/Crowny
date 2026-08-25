#include "cwpch.h"

#include "Crowny/Import/SpecificImporter.h"

#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    String NormalizeImportExtension(StringView extension)
    {
        if (!extension.empty() && extension.front() == '.')
            extension.remove_prefix(1);

        String normalized(extension);
        StringUtils::ToLower(normalized);
        return normalized;
    }

    Ref<ImportOptions> SpecificImporter::CreateImportOptions() const { return CreateRef<ImportOptions>(); }

    Ref<const ImportOptions> SpecificImporter::GetDefaultImportOptions() const
    {
        std::call_once(m_DefaultImportOptionsOnce, [this]() { m_DefaultImportOptions = CreateImportOptions(); });
        return m_DefaultImportOptions;
    }

    Vector<Ref<Asset>> SpecificImporter::ImportAll(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Ref<Asset> asset = Import(path, importOptions);
        return asset != nullptr ? Vector<Ref<Asset>>{ asset } : Vector<Ref<Asset>>{};
    }

} // namespace Crowny
