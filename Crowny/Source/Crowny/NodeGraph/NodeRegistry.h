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

        void Register(StringID typeName, StringID category, FactoryFunc factory);
        Ref<Node> Create(StringID typeName, UUID id) const;
        Ref<Node> Create(StringID typeName) const;

        const Map<StringID, Vector<StringID>>& GetCategorizedTypes() const { return m_CategorizedTypes; }
        bool HasType(StringID typeName) const { return m_Registry.find(typeName) != m_Registry.end(); }

    private:
        struct Entry
        {
            StringID Category;
            FactoryFunc Factory;
        };

        UnorderedMap<StringID, Entry> m_Registry;
        Map<StringID, Vector<StringID>> m_CategorizedTypes;
    };

} // namespace Crowny
