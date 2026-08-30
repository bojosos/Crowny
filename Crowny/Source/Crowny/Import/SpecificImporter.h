#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Import/ImportOptions.h"

#include <mutex>

namespace Crowny
{

    String NormalizeImportExtension(StringView extension);

    enum class ImporterThreadingPolicy
    {
        MainThreadOnly,
        SerializedWorker,
        ParallelWorker
    };

    class SpecificImporter
    {
    public:
        SpecificImporter() = default;
        virtual ~SpecificImporter() = default;

        // Receives an extension without a leading dot, normalized to lowercase by Importer.
        virtual bool IsExtensionSupported(const String& ext) const = 0;

        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const = 0;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) = 0;
        virtual Vector<Ref<Asset>> ImportAll(const Path& path, Ref<const ImportOptions> importOptions);
        virtual Ref<ImportOptions> CreateImportOptions() const;

        // Worker policies are opt-in. ImportAll must be safe under the selected policy.
        virtual ImporterThreadingPolicy GetThreadingPolicy() const { return ImporterThreadingPolicy::MainThreadOnly; }

        Ref<const ImportOptions> GetDefaultImportOptions() const;

    private:
        mutable std::once_flag m_DefaultImportOptionsOnce;
        mutable Ref<const ImportOptions> m_DefaultImportOptions;
    };

} // namespace Crowny
