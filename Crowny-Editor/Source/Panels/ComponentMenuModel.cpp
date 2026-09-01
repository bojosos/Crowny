#include "cwepch.h"

#include "Panels/ComponentMenuModel.h"

#include "Crowny/Common/StringUtils.h"

namespace Crowny
{
    void ComponentMenuModel::AddComponent(ItemId id, String name, String group)
    {
        ComponentEntry entry;
        entry.Id = id;
        entry.Name = std::move(name);
        entry.Group = group.empty() ? "Core" : std::move(group);
        entry.Detail = entry.Group + " component";
        m_Components.push_back(std::move(entry));
        m_ComponentsSorted = false;
        ++m_ComponentCatalogVersion;
    }

    bool ComponentMenuModel::HasScriptCatalog(uint64_t fingerprint) const { return m_HasScriptCatalog && m_ScriptCatalogFingerprint == fingerprint; }

    void ComponentMenuModel::SetScripts(uint64_t fingerprint, Vector<ScriptEntry> scripts)
    {
        if (HasScriptCatalog(fingerprint))
            return;

        for (ScriptEntry& entry : scripts)
        {
            if (entry.Name.empty())
                entry.Name = entry.Identity.TypeName;

            entry.SearchText = entry.Name;
            if (entry.Identity.IsValid())
                entry.SearchText += " " + entry.Identity.GetFullName() + " " + entry.Identity.Assembly;
            if (entry.Detail.empty())
            {
                entry.Detail = entry.Identity.IsValid() ? entry.Identity.GetFullName() + " (" + entry.Identity.Assembly + ")" : "C# script";
            }
        }

        std::sort(scripts.begin(), scripts.end(), [](const ScriptEntry& left, const ScriptEntry& right) {
            if (left.Name != right.Name)
                return left.Name < right.Name;
            if (left.Visible != right.Visible)
                return left.Visible > right.Visible;
            if (left.Identity.Assembly != right.Identity.Assembly)
                return left.Identity.Assembly < right.Identity.Assembly;
            if (left.Identity.Namespace != right.Identity.Namespace)
                return left.Identity.Namespace < right.Identity.Namespace;
            return left.Identity.TypeName < right.Identity.TypeName;
        });
        m_Scripts = std::move(scripts);
        m_ScriptCatalogFingerprint = fingerprint;
        m_HasScriptCatalog = true;
        m_HasSearchResults = false;
    }

    const Vector<ComponentMenuModel::ComponentEntry>& ComponentMenuModel::GetComponents()
    {
        SortComponents();
        return m_Components;
    }

    const Vector<ComponentMenuModel::CategoryEntry>& ComponentMenuModel::GetCategories()
    {
        SortComponents();
        return m_Categories;
    }

    const ComponentMenuModel::SearchResults& ComponentMenuModel::Search(StringView query)
    {
        SortComponents();
        const bool queryMatches = m_Query.size() == query.size() && std::equal(m_Query.begin(), m_Query.end(), query.begin());
        if (m_HasSearchResults && queryMatches && m_ResolvedComponentCatalogVersion == m_ComponentCatalogVersion &&
            m_ResolvedScriptCatalogFingerprint == m_ScriptCatalogFingerprint)
            return m_SearchResults;

        m_Query.assign(query.begin(), query.end());
        m_SearchResults.ComponentIndices.clear();
        m_SearchResults.ScriptIndices.clear();
        m_SearchResults.ScriptNameDeclared = false;

        for (size_t index = 0; index < m_Components.size(); index++)
        {
            const ComponentEntry& entry = m_Components[index];
            if (StringUtils::IsSearchMathing(entry.Name, m_Query) || StringUtils::IsSearchMathing(entry.Group, m_Query))
                m_SearchResults.ComponentIndices.push_back(index);
        }
        std::sort(m_SearchResults.ComponentIndices.begin(), m_SearchResults.ComponentIndices.end(), [&](size_t left, size_t right) {
            const ComponentEntry& leftEntry = m_Components[left];
            const ComponentEntry& rightEntry = m_Components[right];
            if (leftEntry.Name != rightEntry.Name)
                return leftEntry.Name < rightEntry.Name;
            return leftEntry.Id < rightEntry.Id;
        });

        for (size_t index = 0; index < m_Scripts.size(); index++)
        {
            const ScriptEntry& entry = m_Scripts[index];
            if (entry.Name == m_Query)
                m_SearchResults.ScriptNameDeclared = true;
            if (entry.Visible && StringUtils::IsSearchMathing(entry.SearchText, m_Query))
                m_SearchResults.ScriptIndices.push_back(index);
        }

        m_SearchResults.CreateScriptLabel.clear();
        m_SearchResults.CreateScriptLabel.reserve(m_Query.size() + 13u);
        m_SearchResults.CreateScriptLabel += "Create \"";
        m_SearchResults.CreateScriptLabel += m_Query;
        m_SearchResults.CreateScriptLabel += ".cs\"";
        m_ResolvedComponentCatalogVersion = m_ComponentCatalogVersion;
        m_ResolvedScriptCatalogFingerprint = m_ScriptCatalogFingerprint;
        m_HasSearchResults = true;
        return m_SearchResults;
    }

    void ComponentMenuModel::SortComponents()
    {
        if (m_ComponentsSorted)
            return;

        std::sort(m_Components.begin(), m_Components.end(), [](const ComponentEntry& left, const ComponentEntry& right) {
            const bool leftIsCore = left.Group == "Core";
            const bool rightIsCore = right.Group == "Core";
            if (leftIsCore != rightIsCore)
                return leftIsCore;
            if (left.Group != right.Group)
                return left.Group < right.Group;
            if (left.Name != right.Name)
                return left.Name < right.Name;
            return left.Id < right.Id;
        });

        m_Categories.clear();
        for (size_t componentIndex = 0; componentIndex < m_Components.size(); componentIndex++)
        {
            const ComponentEntry& component = m_Components[componentIndex];
            if (m_Categories.empty() || m_Categories.back().Name != component.Group)
                m_Categories.push_back({ component.Group, componentIndex, 1u });
            else
                ++m_Categories.back().ComponentCount;
        }
        m_ComponentsSorted = true;
    }
} // namespace Crowny
