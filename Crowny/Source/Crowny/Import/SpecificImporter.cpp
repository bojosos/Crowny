#include "cwpch.h"

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{

    Ref<ImportOptions> SpecificImporter::CreateImportOptions() const { return CreateRef<ImportOptions>(); }

    Ref<const ImportOptions> SpecificImporter::GetDefaultImportOptions() const
    {
        if (m_DefaultImportOptions == nullptr)
            m_DefaultImportOptions = CreateImportOptions();
        return m_DefaultImportOptions;
    }

} // namespace Crowny