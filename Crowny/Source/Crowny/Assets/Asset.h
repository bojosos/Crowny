#pragma once

namespace Crowny
{

    class Asset
    {
    public:
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

    protected:
        void AddDependency(const Ref<Asset>& asset);
        const std::vector<Ref<Asset>>& GetDependencies() const;
        bool m_KeepData;
        std::string m_Name;
        std::vector<Ref<Asset>> m_Dependencies;
    };

} // namespace Crowny