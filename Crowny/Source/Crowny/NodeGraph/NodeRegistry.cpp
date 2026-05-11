#include "cwpch.h"

#include "Crowny/NodeGraph/NodeRegistry.h"

namespace Crowny
{
    NodeRegistry& NodeRegistry::Get()
    {
        static NodeRegistry instance;
        return instance;
    }

    void NodeRegistry::Register(StringID typeName, StringID category, FactoryFunc factory)
    {
        if (m_Registry.find(typeName) != m_Registry.end())
            return; // Already registered (guard against duplicate calls)
        m_Registry[typeName] = { category, std::move(factory) };
        m_CategorizedTypes[category].push_back(typeName);
    }

    Ref<Node> NodeRegistry::Create(StringID typeName, UUID id) const
    {
        const auto it = m_Registry.find(typeName);
        if (it == m_Registry.end())
        {
            CW_ENGINE_ERROR("NodeRegistry: Unknown node type '{0}'", typeName.c_str());
            return nullptr;
        }
        return it->second.Factory(id);
    }

    Ref<Node> NodeRegistry::Create(StringID typeName) const { return Create(typeName, UuidGenerator::Generate()); }

} // namespace Crowny
