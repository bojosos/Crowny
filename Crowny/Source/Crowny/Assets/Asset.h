#pragma once

// TODO: Move from here to common as I need to serialize much more
#define CW_SERIALIZABLE(...)                                                                                           \
    template <typename Archive> friend void save(Archive& ar, const __VA_ARGS__& asset);                               \
    template <typename Archive> friend void load(Archive& ar, __VA_ARGS__& asset);

namespace Crowny
{

    class Asset
    {
    public:
        Asset() = default;
        virtual ~Asset() = default;

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

    protected:
        CW_SERIALIZABLE(Asset);
        void AddDependency(const Ref<Asset>& asset);
        const std::vector<Ref<Asset>>& GetDependencies() const;

        bool m_KeepData;
        std::string m_Name;
        std::vector<Ref<Asset>> m_Dependencies;
    };

} // namespace Crowny