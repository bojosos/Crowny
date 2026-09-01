#include "cwtpch.h"

#include "Panels/ScriptInspectorProgressBar.h"

using namespace Crowny;

TEST_CASE("Progress bar resolves state-backed presentation values", "[Editor][Scripting][ProgressBar]")
{
    ScriptProgressBarSettings settings;
    settings.Min = -10.0;
    settings.Max = 10.0;
    settings.MinGetter = "Minimum";
    settings.MaxGetter = "Maximum";
    settings.ColorGetter = "BarColor";
    settings.CustomValueStringGetter = "BarLabel";

    ScriptValue color;
    color.Kind = ScriptValueKind::Color;
    color.VectorValue = glm::vec4(0.2f, 0.4f, 0.8f, 0.75f);
    const ScriptValue root = ScriptValue::Object({ { "Minimum", ScriptValue::Signed(0) },
                                                   { "Maximum", ScriptValue::Float(250.0) },
                                                   { "BarColor", color },
                                                   { "BarLabel", ScriptValue::Text("75 / 250") } });

    double min = 0.0;
    double max = 0.0;
    ScriptInspectorProgressBar::ResolveBounds(settings, root, min, max);
    CHECK(min == 0.0);
    CHECK(max == 250.0);
    CHECK(ScriptInspectorProgressBar::ResolveColor(settings.ColorGetter, root, glm::vec4(1.0f)) == color.VectorValue);
    CHECK(ScriptInspectorProgressBar::ResolveLabel(settings, root, ScriptValue::Float(75.0)) == "75 / 250");
    CHECK(ScriptInspectorProgressBar::Fraction(75.0, min, max) == Approx(0.3f));
}

TEST_CASE("Progress bar reads and edits supported numeric values", "[Editor][Scripting][ProgressBar]")
{
    ScriptValue signedValue = ScriptValue::Signed(4);
    CHECK(ScriptInspectorProgressBar::TryWriteNumber(signedValue, 7.6));
    CHECK(signedValue.SignedValue == 8);

    ScriptValue unsignedValue = ScriptValue::Unsigned(4);
    CHECK(ScriptInspectorProgressBar::TryWriteNumber(unsignedValue, -5.0));
    CHECK(unsignedValue.UnsignedValue == 0);

    ScriptValue decimalValue;
    decimalValue.Kind = ScriptValueKind::Decimal;
    decimalValue.StringValue = "12.5";
    double number = 0.0;
    REQUIRE(ScriptInspectorProgressBar::TryReadNumber(decimalValue, number));
    CHECK(number == Approx(12.5));
    CHECK(ScriptInspectorProgressBar::TryWriteNumber(decimalValue, 42.25));
    REQUIRE(ScriptInspectorProgressBar::TryReadNumber(decimalValue, number));
    CHECK(number == Approx(42.25));

    CHECK(ScriptInspectorProgressBar::Fraction(-1.0, 0.0, 10.0) == 0.0f);
    CHECK(ScriptInspectorProgressBar::Fraction(20.0, 0.0, 10.0) == 1.0f);
    CHECK(ScriptInspectorProgressBar::Fraction(5.0, 5.0, 5.0) == 0.0f);
}
