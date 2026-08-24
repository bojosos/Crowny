#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Crowny/Common/StringUtils.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Panels/NodeEditor/NodeEditorPalette.h"
#include "UI/UIUtils.h"

#include <imgui.h>

namespace Crowny
{
    namespace
    {
        struct PaletteItem
        {
            StringID TypeName;
            String Category;
            String DisplayName;
            String SearchText;
        };

        Vector<PaletteItem> BuildPaletteItems()
        {
            Vector<PaletteItem> items;
            for (const auto& [category, typeNames] : NodeRegistry::Get().GetCategorizedTypes())
            {
                for (const StringID typeName : typeNames)
                {
                    const Ref<Node> node = NodeRegistry::Get().Create(typeName);
                    const String displayName = node ? node->GetDisplayName().c_str() : typeName.c_str();
                    String searchText = displayName + " " + category.c_str() + " " + typeName.c_str();
                    StringUtils::ToLower(searchText);
                    items.push_back({ typeName, category.c_str(), displayName, std::move(searchText) });
                }
            }

            std::sort(items.begin(), items.end(), [](const PaletteItem& lhs, const PaletteItem& rhs) {
                if (lhs.Category != rhs.Category)
                    return lhs.Category < rhs.Category;
                return lhs.DisplayName < rhs.DisplayName;
            });
            return items;
        }
    } // namespace

    void RenderNodePalette(String& searchString, bool& grabFocus, const std::function<void(StringID)>& createNode)
    {
        if (ImGui::IsWindowAppearing())
        {
            searchString.clear();
            grabFocus = true;
        }

        ImGui::SetNextItemWidth(340.0f);
        UIUtils::SearchWidget(searchString, "Search nodes...", &grabFocus);
        ImGui::Separator();

        static const Vector<PaletteItem> paletteItems = BuildPaletteItems();
        String normalizedSearch = searchString;
        StringUtils::ToLower(normalizedSearch);

        uint32_t matchCount = 0;
        String currentCategory;
        const float resultHeight = ImGui::GetTextLineHeightWithSpacing() * 13.0f;
        if (ImGui::BeginChild("##NodePaletteResults", ImVec2(340.0f, resultHeight), false))
        {
            for (const PaletteItem& item : paletteItems)
            {
                if (!normalizedSearch.empty() && item.SearchText.find(normalizedSearch) == String::npos)
                    continue;

                if (item.Category != currentCategory)
                {
                    if (!currentCategory.empty())
                        ImGui::Spacing();
                    currentCategory = item.Category;
                    ImGui::TextDisabled("%s", currentCategory.c_str());
                }

                ImGui::PushID(item.TypeName.c_str());
                if (ImGui::Selectable(item.DisplayName.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 0.0f)))
                {
                    createNode(item.TypeName);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                matchCount++;
            }

            if (matchCount == 0)
            {
                ImGui::Spacing();
                ImGui::TextDisabled("No nodes match \"%s\".", searchString.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("%u %s", matchCount, matchCount == 1 ? "node" : "nodes");
    }
} // namespace Crowny

#endif // CW_WITH_NODES
