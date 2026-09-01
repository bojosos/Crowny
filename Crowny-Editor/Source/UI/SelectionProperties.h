#pragma once

#include "Editor/SelectionProperty.h"
#include "UI/Properties.h"
#include "UI/SelectionPropertyLayout.h"

#include <type_traits>

namespace Crowny::UI
{
    template <SelectionPropertyBinding Property, typename Drawer> static bool EditSelectionProperty(Property&& property, Drawer&& drawer)
    {
        using Value = typename std::remove_cvref_t<Property>::ValueType;
        SelectionPropertyValue<Value> state = property.Read();
        if (!state)
            return false;

        Value value = *state.Primary;
        if constexpr (std::is_same_v<Value, String>)
        {
            if (state.Mixed)
                value.clear();
        }

        if (state.Mixed)
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        const bool changed = std::invoke(std::forward<Drawer>(drawer), value);
        if (state.Mixed)
            ImGui::PopItemFlag();

        if (changed)
            property.Assign(value);
        return changed;
    }

    template <typename T> struct SelectionVectorLength : std::integral_constant<size_t, 0u>
    {
    };
    template <> struct SelectionVectorLength<glm::vec2> : std::integral_constant<size_t, 2u>
    {
    };
    template <> struct SelectionVectorLength<glm::vec3> : std::integral_constant<size_t, 3u>
    {
    };
    template <> struct SelectionVectorLength<glm::vec4> : std::integral_constant<size_t, 4u>
    {
    };

    struct SelectionVectorPropertyOptions
    {
        float Speed = 0.1f;
        float Minimum = 0.0f;
        float Maximum = 0.0f;
        std::optional<float> ResetValue;
    };

    inline SelectionVectorLayout ConfigureSelectionVectorColumns(size_t axisCount)
    {
        if (ImGui::GetColumnsCount() < 2)
            return CalculateSelectionVectorLayout(ImGui::GetContentRegionAvail().x, axisCount, ImGui::GetFrameHeight());

        const float totalWidth = ImGui::GetColumnWidth(0) + ImGui::GetColumnWidth(1);
        const SelectionVectorLayout layout = CalculateSelectionVectorLayout(totalWidth, axisCount, ImGui::GetFrameHeight());
        ImGui::SetColumnWidth(0, layout.LabelWidth);
        return layout;
    }

    template <SelectionPropertyBinding Property, typename... Args>
        requires(SelectionVectorLength<typename std::remove_cvref_t<Property>::ValueType>::value == 0u)
    static bool Property(const char* label, Property&& property, Args&&... args)
    {
        return EditSelectionProperty(std::forward<Property>(property),
                                     [&](auto& value) { return Property(label, value, std::forward<Args>(args)...); });
    }

    template <SelectionPropertyBinding Property>
        requires(SelectionVectorLength<typename std::remove_cvref_t<Property>::ValueType>::value != 0u)
    static bool Property(const char* label, Property&& property, const SelectionVectorPropertyOptions& options)
    {
        constexpr size_t length = SelectionVectorLength<typename std::remove_cvref_t<Property>::ValueType>::value;
        constexpr const char* formats[] = { "X %.3f", "Y %.3f", "Z %.3f", "W %.3f" };
        constexpr const char* axisLabels[] = { "X", "Y", "Z", "W" };
        constexpr ImU32 axisColors[] = { Colors::AxisX, Colors::AxisY, Colors::AxisZ, Colors::Accent };
        constexpr ImU32 axisHoverColors[] = { Colors::AxisXHover, Colors::AxisYHover, Colors::AxisZHover, Colors::AccentHover };

        bool changed = false;
        Pre(label);
        ImGui::PushID(label);
        ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
        constexpr float axisSpacing = 3.0f;
        constexpr float resetSpacing = 1.0f;
        const float resetWidth = options.ResetValue ? ImGui::GetFrameHeight() : 0.0f;
        const float valueWidth = ImGui::GetContentRegionAvail().x;
        const float axisWidth =
          std::max(1.0f, (valueWidth - static_cast<float>(length - 1u) * axisSpacing) / static_cast<float>(length));
        const float inputWidth = std::max(1.0f, axisWidth - resetWidth - (options.ResetValue ? resetSpacing : 0.0f));
        for (size_t axis = 0u; axis < length; ++axis)
        {
            if (axis != 0u)
                ImGui::SameLine(0.0f, axisSpacing);

            auto axisProperty = property.Element(axis);
            SelectionPropertyValue<float> state = axisProperty.Read();
            if (!state)
                continue;

            ImGui::PushID(static_cast<int>(axis));
            bool resetChanged = false;
            if (options.ResetValue)
            {
                ScopedColor buttonColor(ImGuiCol_Button, axisColors[axis]);
                ScopedColor buttonHover(ImGuiCol_ButtonHovered, axisHoverColors[axis]);
                ScopedColor buttonActive(ImGuiCol_ButtonActive, axisColors[axis]);
                ScopedStyle buttonRounding(ImGuiStyleVar_FrameRounding, 1.0f);
                ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
                bool resetClicked = false;
                {
                    ScopedFont boldFont(ScopedFont::Bold);
                    resetClicked = ImGui::Button(axisLabels[axis], ImVec2(resetWidth, 0.0f));
                }
                ImGui::PopItemFlag();
                const SelectionPropertyWrite resetWrite = resetClicked ? axisProperty.Assign(*options.ResetValue) : SelectionPropertyWrite{};
                UndoRedo::Get().OnItemInteract(static_cast<bool>(resetWrite));
                resetChanged = static_cast<bool>(resetWrite);
                changed |= resetChanged;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Reset %s to %.3f", axisLabels[axis], *options.ResetValue);
                ImGui::SameLine(0.0f, resetSpacing);
            }

            ImGui::SetNextItemWidth(inputWidth);
            const bool inputMixed = state.Mixed && !resetChanged;
            if (inputMixed)
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
            float value = resetChanged ? *options.ResetValue : *state.Primary;
            const char* format = options.ResetValue ? "%.3f" : formats[axis];
            const bool axisChanged = DragFloat("##axis", &value, options.Speed, options.Minimum, options.Maximum, format);
            const SelectionPropertyWrite axisWrite = axisChanged ? axisProperty.Assign(value) : SelectionPropertyWrite{};
            UndoRedo::Get().OnItemInteract(static_cast<bool>(axisWrite));
            if (inputMixed)
                ImGui::PopItemFlag();
            if (axis + 1u != length && !IsItemDisabled())
                DrawItemActivityOutline(2.0f, true, Colors::Accent);
            ImGui::PopID();

            changed |= static_cast<bool>(axisWrite);
        }
        ImGui::PopID();
        Post();
        return changed;
    }

    template <SelectionPropertyBinding Property>
        requires(SelectionVectorLength<typename std::remove_cvref_t<Property>::ValueType>::value != 0u)
    static bool Property(const char* label, Property&& property, float speed = 0.1f, float minimum = 0.0f, float maximum = 0.0f)
    {
        return Property(label, std::forward<Property>(property), SelectionVectorPropertyOptions{ speed, minimum, maximum });
    }

    template <SelectionPropertyBinding Property> static bool PropertyColor(const char* label, Property&& property, ImGuiColorEditFlags flags = 0)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyColor(label, value, flags); });
    }

    template <SelectionPropertyBinding Property> static bool PropertyColor(const char* label, Property&& property, bool showAlpha, bool hdr)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyColor(label, value, showAlpha, hdr); });
    }

    template <SelectionPropertyBinding Property> static bool PropertyMultiline(const char* label, Property&& property, int32_t lines = 8)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyMultiline(label, value, lines); });
    }

    template <SelectionPropertyBinding Property, typename Minimum, typename Maximum>
    static bool PropertySlider(const char* label, Property&& property, Minimum minimum, Maximum maximum)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertySlider(label, value, minimum, maximum); });
    }

    template <SelectionPropertyBinding Property>
    static bool PropertyDropdown(const char* label, std::initializer_list<const char*> options, Property&& property)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyDropdown(label, options, value); });
    }

    template <SelectionPropertyBinding Property> static bool PropertyDropdown(const char* label, const Vector<String>& options, Property&& property)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyDropdown(label, options, value); });
    }

    template <SelectionPropertyBinding Property>
    static bool PropertyIconTabs(const char* label, std::initializer_list<PropertyIconTab> tabs, Property&& property)
    {
        return EditSelectionProperty(std::forward<Property>(property), [&](auto& value) { return PropertyIconTabs(label, tabs, value); });
    }

    template <typename Enum, SelectionPropertyBinding Property>
    static bool PropertyFlags(const char* label, std::initializer_list<const char*> buttonLabels, std::initializer_list<Enum> bits,
                              Property&& property)
    {
        using FlagsType = typename std::remove_cvref_t<Property>::ValueType;
        if (buttonLabels.size() != bits.size() || bits.size() == 0u)
            return false;

        Pre(label);
        ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        const float buttonWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x / static_cast<float>(bits.size()));
        bool changed = false;
        auto bit = bits.begin();
        uint32_t index = 0u;
        for (const char* buttonLabel : buttonLabels)
        {
            const Enum currentBit = *bit++;
            auto bitProperty = property.Project([currentBit](const FlagsType& value) { return value.IsSet(currentBit); },
                                                [currentBit](FlagsType& value, bool set) {
                                                    if (set)
                                                        value.Set(currentBit);
                                                    else
                                                        value.Unset(currentBit);
                                                });
            const SelectionPropertyValue<bool> state = bitProperty.Read();
            const bool primarySet = state.Primary.value_or(false);
            const String id = String(buttonLabel) + "##" + label + std::to_string(index);
            if (!state.Mixed && primarySet)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_ButtonActive);
            const bool buttonChanged = ImGui::Button(id.c_str(), ImVec2(buttonWidth, 0.0f));
            if (buttonChanged)
            {
                bitProperty.Assign(state.Mixed || !primarySet);
                changed = true;
            }
            if (!state.Mixed && primarySet)
                ImGui::PopStyleColor();
            if (state.Mixed)
            {
                const ImRect bounds = GetItemRect();
                ImGui::GetWindowDrawList()->AddLine(ImVec2(bounds.Min.x + 7.0f, bounds.GetCenter().y),
                                                    ImVec2(bounds.Max.x - 7.0f, bounds.GetCenter().y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                                    1.5f);
            }
            UndoRedo::Get().OnItemInteract(buttonChanged);
            if (++index < bits.size())
                ImGui::SameLine(0.0f, 0.0f);
        }
        Post();
        return changed;
    }

    template <typename AssetType, SelectionPropertyBinding Property>
        requires(std::is_same_v<typename std::remove_cvref_t<Property>::ValueType, AssetHandle<AssetType>>)
    static bool PropertyAsset(const char* label, Property&& property)
    {
        return EditSelectionProperty(std::forward<Property>(property),
                                     [&](AssetHandle<AssetType>& value) { return UIUtils::AssetReference<AssetType>(label, value); });
    }
} // namespace Crowny::UI
