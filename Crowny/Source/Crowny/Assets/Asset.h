#pragma once

#include "Crowny/Common/Uuid.h"

#include <cereal/types/polymorphic.hpp>

namespace Crowny
{

    class ImportOptions;

    class Asset
    {
    public:
        Asset() = default;
        virtual ~Asset() = default;
        virtual void Init(){};
        const String& GetName() const { return m_Name; }
        void SetName(const String& name) { m_Name = name; }

    protected:
        CW_SERIALIZABLE(Asset);
        void AddDependency(const Ref<Asset>& asset);
        const Vector<Ref<Asset>>& GetDependencies() const;

        bool m_KeepData = true;
        String m_Name;
        Vector<Ref<Asset>> m_Dependencies;
    };

    struct AssetMetadata
    {
        // TODO: Preview icons, with different sizes (256....16)
        UUID Uuid;                        // Asset UUID
        Ref<ImportOptions> ImportOptions; // Asset import options
        bool IncludeInBuild;
    };

} // namespace Crowny