#include "cwepch.h"

#include "Panels/ScriptInspectorSearch.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>

namespace Crowny::ScriptInspectorSearch
{
    namespace
    {
        bool HasOption(const ScriptSearchSettings& settings, ScriptSearchFilterOptions option)
        {
            return (settings.FilterOptions & option) != ScriptSearchFilterOptions::None;
        }

        bool IsSeparator(unsigned char character) { return std::isspace(character) != 0 || character == '_'; }

        bool MatchesText(StringView text, StringView query, bool fuzzy)
        {
            if (query.empty())
                return true;
            if (text.empty())
                return false;

            if (!fuzzy)
            {
                return std::search(text.begin(), text.end(), query.begin(), query.end(),
                                   [](unsigned char left, unsigned char right) { return std::tolower(left) == std::tolower(right); }) != text.end();
            }

            size_t queryOffset = 0;
            for (const unsigned char character : text)
            {
                while (queryOffset < query.size() && IsSeparator(static_cast<unsigned char>(query[queryOffset])))
                    ++queryOffset;
                if (queryOffset == query.size())
                    return true;
                if (IsSeparator(character))
                    continue;
                if (std::tolower(character) == std::tolower(static_cast<unsigned char>(query[queryOffset])))
                    ++queryOffset;
            }
            while (queryOffset < query.size() && IsSeparator(static_cast<unsigned char>(query[queryOffset])))
                ++queryOffset;
            return queryOffset == query.size();
        }

        String NiceName(StringView name)
        {
            String result;
            result.reserve(name.size() + 4);
            for (size_t index = 0; index < name.size(); ++index)
            {
                const unsigned char character = static_cast<unsigned char>(name[index]);
                if (character == '_')
                {
                    if (!result.empty() && result.back() != ' ')
                        result.push_back(' ');
                    continue;
                }
                const bool upper = std::isupper(character) != 0;
                const bool previousLower = index != 0 && std::islower(static_cast<unsigned char>(name[index - 1])) != 0;
                const bool nextLower = index + 1 < name.size() && std::islower(static_cast<unsigned char>(name[index + 1])) != 0;
                const bool previousUpper = index != 0 && std::isupper(static_cast<unsigned char>(name[index - 1])) != 0;
                if (upper && !result.empty() && result.back() != ' ' && (previousLower || previousUpper && nextLower))
                    result.push_back(' ');
                result.push_back(static_cast<char>(character));
            }
            return result;
        }

        const char* KindName(ScriptValueKind kind)
        {
            switch (kind)
            {
            case ScriptValueKind::Null:
                return "Null";
            case ScriptValueKind::Boolean:
                return "Boolean";
            case ScriptValueKind::SignedInteger:
                return "Signed Integer";
            case ScriptValueKind::UnsignedInteger:
                return "Unsigned Integer";
            case ScriptValueKind::Float:
                return "Float";
            case ScriptValueKind::Decimal:
                return "Decimal";
            case ScriptValueKind::String:
                return "String";
            case ScriptValueKind::Enum:
                return "Enum";
            case ScriptValueKind::Vector2:
                return "Vector2";
            case ScriptValueKind::Vector3:
                return "Vector3";
            case ScriptValueKind::Vector4:
                return "Vector4";
            case ScriptValueKind::Color:
                return "Color";
            case ScriptValueKind::Quaternion:
                return "Quaternion";
            case ScriptValueKind::Matrix4:
                return "Matrix4";
            case ScriptValueKind::Entity:
                return "Entity";
            case ScriptValueKind::Component:
                return "Component";
            case ScriptValueKind::Asset:
                return "Asset";
            case ScriptValueKind::Array:
                return "Array";
            case ScriptValueKind::List:
                return "List";
            case ScriptValueKind::Dictionary:
                return "Dictionary";
            case ScriptValueKind::Object:
                return "Object";
            case ScriptValueKind::Uuid:
                return "UUID";
            }
            return "";
        }

        StringView ValueText(const ScriptValue& value)
        {
            thread_local char buffer[256];
            const auto formatNumber = [&](auto number) -> StringView {
                const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), number);
                return result.ec == std::errc{} ? StringView(buffer, static_cast<size_t>(result.ptr - buffer)) : StringView();
            };
            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                return "null";
            case ScriptValueKind::Boolean:
                return value.BooleanValue ? "true" : "false";
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum:
                return formatNumber(value.SignedValue);
            case ScriptValueKind::UnsignedInteger:
                return formatNumber(value.UnsignedValue);
            case ScriptValueKind::Float:
                return formatNumber(value.FloatingValue);
            case ScriptValueKind::Decimal:
            case ScriptValueKind::String:
                return value.StringValue;
            case ScriptValueKind::Entity:
            case ScriptValueKind::Component:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid:
                std::snprintf(buffer, sizeof(buffer), "%s", value.ReferenceValue.ToTextBuffer().data());
                return buffer;
            case ScriptValueKind::Vector2:
                std::snprintf(buffer, sizeof(buffer), "%g %g", value.VectorValue.x, value.VectorValue.y);
                return buffer;
            case ScriptValueKind::Vector3:
                std::snprintf(buffer, sizeof(buffer), "%g %g %g", value.VectorValue.x, value.VectorValue.y, value.VectorValue.z);
                return buffer;
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Color:
            case ScriptValueKind::Quaternion:
                std::snprintf(buffer, sizeof(buffer), "%g %g %g %g", value.VectorValue.x, value.VectorValue.y, value.VectorValue.z,
                              value.VectorValue.w);
                return buffer;
            case ScriptValueKind::Matrix4:
            case ScriptValueKind::Array:
            case ScriptValueKind::List:
            case ScriptValueKind::Dictionary:
            case ScriptValueKind::Object:
                return {};
            }
            return {};
        }

        bool MatchesDirect(StringView propertyName, const ScriptValue& value, StringView query, const ScriptSearchSettings& settings,
                           ScriptValueKind declaredKind, const ScriptTypeIdentity* declaredType)
        {
            if (HasOption(settings, ScriptSearchFilterOptions::PropertyName) && MatchesText(propertyName, query, settings.FuzzySearch))
                return true;
            if (HasOption(settings, ScriptSearchFilterOptions::PropertyNiceName) && MatchesText(NiceName(propertyName), query, settings.FuzzySearch))
                return true;
            if (HasOption(settings, ScriptSearchFilterOptions::TypeOfValue))
            {
                const ScriptValueKind kind = declaredKind == ScriptValueKind::Null ? value.Kind : declaredKind;
                if (MatchesText(KindName(kind), query, settings.FuzzySearch))
                    return true;
                const ScriptTypeIdentity* type = declaredType != nullptr && declaredType->IsValid() ? declaredType : &value.DeclaredType;
                if (type->IsValid() &&
                    (MatchesText(type->TypeName, query, settings.FuzzySearch) || MatchesText(type->GetFullName(), query, settings.FuzzySearch)))
                    return true;
            }
            return HasOption(settings, ScriptSearchFilterOptions::ValueToString) && MatchesText(ValueText(value), query, settings.FuzzySearch);
        }

        bool MatchesRecursive(StringView propertyName, const ScriptValue& value, StringView query, const ScriptSearchSettings& settings,
                              ScriptValueKind declaredKind, const ScriptTypeIdentity* declaredType, bool recurse)
        {
            if (MatchesDirect(propertyName, value, query, settings, declaredKind, declaredType))
                return true;
            if (!recurse)
                return false;
            for (const auto& [name, member] : value.Members)
            {
                if (MatchesRecursive(name, member, query, settings, member.Kind, &member.DeclaredType, true))
                    return true;
            }
            for (size_t index = 0; index < value.Elements.size(); ++index)
            {
                char name[32];
                std::snprintf(name, sizeof(name), "Element %zu", index);
                const ScriptValue& element = value.Elements[index];
                if (MatchesRecursive(name, element, query, settings, element.Kind, &element.DeclaredType, true))
                    return true;
            }
            return false;
        }
    } // namespace

    bool Matches(StringView propertyName, const ScriptValue& value, StringView query, const ScriptSearchSettings& settings,
                 ScriptValueKind declaredKind, const ScriptTypeIdentity* declaredType)
    {
        return query.empty() || MatchesRecursive(propertyName, value, query, settings, declaredKind, declaredType, settings.Recursive);
    }
} // namespace Crowny::ScriptInspectorSearch
