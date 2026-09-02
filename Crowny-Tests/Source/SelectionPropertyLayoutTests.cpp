#include "UI/SelectionPropertyLayout.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Transform selection layout keeps a historical label column and usable axis inputs", "[Editor][Inspector][Layout]")
{
    const UI::SelectionVectorLayout roomy = UI::CalculateSelectionVectorLayout(360.0f, 3u, 22.0f);
    CHECK(roomy.LabelWidth == Catch::Approx(100.0f));
    CHECK(roomy.InputWidth > 50.0f);
    CHECK(roomy.FitsMinimumInputs);

    const UI::SelectionVectorLayout constrained = UI::CalculateSelectionVectorLayout(231.0f, 3u, 22.0f);
    CHECK(constrained.LabelWidth == Catch::Approx(72.0f));
    CHECK(constrained.InputWidth == Catch::Approx(28.0f));
    CHECK(constrained.FitsMinimumInputs);
}

TEST_CASE("Transform selection layout reports widths that cannot fit three reset and input pairs", "[Editor][Inspector][Layout]")
{
    const UI::SelectionVectorLayout tooNarrow = UI::CalculateSelectionVectorLayout(200.0f, 3u, 22.0f);
    CHECK(tooNarrow.LabelWidth == Catch::Approx(72.0f));
    CHECK(tooNarrow.InputWidth < 28.0f);
    CHECK_FALSE(tooNarrow.FitsMinimumInputs);
}
