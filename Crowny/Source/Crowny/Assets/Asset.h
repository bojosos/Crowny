#pragma once

#include "Crowny/Common/Uuid.h"

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

        bool m_KeepData;
        String m_Name;
        Vector<Ref<Asset>> m_Dependencies;
    };

    class AssetMetaData
    {
    private:
        // TODO: Preview icons, with different sizes (128....32)
        UUID m_Uuid;                        // Resource UUID
        Ref<ImportOptions> m_ImportOptions; // Resource import options
        bool m_IncludeInBuild;
    };

} // namespace Crowny