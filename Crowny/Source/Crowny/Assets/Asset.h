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

    struct AssetMetadata
    {
        // TODO: Preview icons, with different sizes (128....32)
        UUID Uuid;                        // Asset UUID
        Ref<ImportOptions> ImportOptions; // Asset import options
        bool IncludeInBuild;
    };

    class ScriptCode : public Asset
    {
    public:
        ScriptCode(const String& source) : m_Source(source) {}

    private:
        String m_Source;
    };

} // namespace Crowny