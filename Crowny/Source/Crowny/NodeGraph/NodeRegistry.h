#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class NodeRegistry
    {
    public:
        using FactoryFunc = std::function<Ref<Node>(UUID id)>;

        static NodeRegistry& Get();

        void Register(StringID typeName, const String& category, FactoryFunc factory);
        Ref<Node> Create(StringID typeName, UUID id) const;
        Ref<Node> Create(StringID typeName) const;

        const Map<String, Vector<String>>& GetCategorizedTypes() const { return m_CategorizedTypes; }
        bool HasType(StringID typeName) const { return m_Registry.find(typeName) != m_Registry.end(); }

    private:
        struct Entry
        {
            String Category;
            FactoryFunc Factory;
        };

        UnorderedMap<StringID, Entry> m_Registry;
        Map<String, Vector<String>> m_CategorizedTypes;
    };

#define CW_REGISTER_NODE(TypeName, Category, ClassName)                                                                                              \
    static bool s_##ClassName##_Registered = []() {                                                                                                  \
        NodeRegistry::Get().Register(TypeName, Category, [](UUID id) -> Ref<Node> { return CreateRef<ClassName>(id); });                             \
        return true;                                                                                                                                 \
    }();

} // namespace Crowny
