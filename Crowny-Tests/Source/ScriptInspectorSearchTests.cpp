#include <catch2/catch_test_macros.hpp>

#include "Panels/ScriptInspectorSearch.h"

using namespace Crowny;

namespace
{
    ScriptValue MakePerk()
    {
        ScriptValue effect = ScriptValue::Object({ { "Skill", ScriptValue::Text("Wisdom") }, { "Value", ScriptValue::Signed(2) } });
        ScriptValue effects;
        effects.Kind = ScriptValueKind::List;
        effects.Elements.push_back(std::move(effect));
        return ScriptValue::Object({ { "Name", ScriptValue::Text("Old Sage") }, { "Effects", std::move(effects) } },
                                   { "GameAssembly", "Game", "Perk" });
    }
} // namespace

TEST_CASE("Searchable inspector values support fuzzy and recursive matching", "[Editor][Scripting][Searchable]")
{
    const ScriptValue perk = MakePerk();
    ScriptSearchSettings settings;

    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "odsge", settings));
    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "wisdom", settings));

    settings.Recursive = false;
    CHECK_FALSE(ScriptInspectorSearch::Matches("Element 0", perk, "wisdom", settings));

    settings.Recursive = true;
    settings.FuzzySearch = false;
    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "Old Sage", settings));
    CHECK_FALSE(ScriptInspectorSearch::Matches("Element 0", perk, "odsge", settings));
}

TEST_CASE("Searchable inspector values honor filter options", "[Editor][Scripting][Searchable]")
{
    const ScriptValue perk = MakePerk();
    ScriptSearchSettings settings;
    settings.FuzzySearch = false;
    settings.FilterOptions = ScriptSearchFilterOptions::PropertyName;

    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "Effects", settings));
    CHECK_FALSE(ScriptInspectorSearch::Matches("Element 0", perk, "Wisdom", settings));

    settings.FilterOptions = ScriptSearchFilterOptions::PropertyNiceName;
    CHECK(ScriptInspectorSearch::Matches("MovementSpeed", ScriptValue::Float(3.0), "Movement Speed", settings));

    settings.FilterOptions = ScriptSearchFilterOptions::TypeOfValue;
    const ScriptTypeIdentity perkType{ "GameAssembly", "Game", "Perk" };
    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "Game.Perk", settings, ScriptValueKind::Object, &perkType));

    settings.FilterOptions = ScriptSearchFilterOptions::ValueToString;
    CHECK(ScriptInspectorSearch::Matches("Element 0", perk, "Wisdom", settings));
    CHECK_FALSE(ScriptInspectorSearch::Matches("Element 0", perk, "Effects", settings));
}
