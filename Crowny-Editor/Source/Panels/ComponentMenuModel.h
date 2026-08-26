#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <cstdint>

namespace Crowny
{
    class ComponentMenuModel
    {
    public:
        using ItemId = uint64_t;

        struct ComponentEntry
        {
            ItemId Id = 0;
            String Name;
            String Group;
            String Detail;
        };

        struct ScriptEntry
        {
            String Name;
            bool Visible = true;
        };

        struct SearchResults
        {
            Vector<size_t> ComponentIndices;
            Vector<size_t> ScriptIndices;
            String CreateScriptLabel;
            bool ScriptNameDeclared = false;

            size_t GetMatchCount() const { return ComponentIndices.size() + ScriptIndices.size(); }
        };

        void AddComponent(ItemId id, String name, String group);

        bool HasScriptCatalog(uint64_t fingerprint) const;
        void SetScripts(uint64_t fingerprint, Vector<ScriptEntry> scripts);

        const Vector<ComponentEntry>& GetComponents();
        const Vector<ScriptEntry>& GetScripts() const { return m_Scripts; }
        const SearchResults& Search(StringView query);

    private:
        void SortComponents();

        Vector<ComponentEntry> m_Components;
        Vector<ScriptEntry> m_Scripts;
        SearchResults m_SearchResults;
        String m_Query;
        uint64_t m_ComponentCatalogVersion = 0;
        uint64_t m_ResolvedComponentCatalogVersion = 0;
        uint64_t m_ScriptCatalogFingerprint = 0;
        uint64_t m_ResolvedScriptCatalogFingerprint = 0;
        bool m_ComponentsSorted = true;
        bool m_HasScriptCatalog = false;
        bool m_HasSearchResults = false;
    };
} // namespace Crowny
