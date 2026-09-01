#include "cwtpch.h"

#include "UI/EnumButtonsModel.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Enum buttons select one ordinary enum value", "[Editor][Inspector][EnumButtons]")
{
    CHECK(EnumButtonsModel::IsSelected(2, 2, false));
    CHECK_FALSE(EnumButtonsModel::IsSelected(3, 2, false));
    CHECK(EnumButtonsModel::Select(2, 3, false) == 3);
}

TEST_CASE("Enum buttons toggle flags and composite values", "[Editor][Inspector][EnumButtons]")
{
    CHECK(EnumButtonsModel::IsSelected(0, 0, true));
    CHECK_FALSE(EnumButtonsModel::IsSelected(1, 0, true));
    CHECK(EnumButtonsModel::Select(3, 0, true) == 0);

    CHECK(EnumButtonsModel::Select(1, 2, true) == 3);
    CHECK(EnumButtonsModel::Select(3, 2, true) == 1);

    CHECK(EnumButtonsModel::IsSelected(7, 3, true));
    CHECK_FALSE(EnumButtonsModel::IsSelected(1, 3, true));
    CHECK(EnumButtonsModel::Select(1, 3, true) == 3);
    CHECK(EnumButtonsModel::Select(7, 3, true) == 4);
}
