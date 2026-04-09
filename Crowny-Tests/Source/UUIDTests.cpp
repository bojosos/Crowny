#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/Uuid.h"

using namespace Crowny;

TEST_CASE("UUID::Basic", "[UUID]")
{
    SECTION("Default constructor creates empty UUID")
    {
        UUID uuid;
        CHECK(uuid.Empty());
        CHECK(uuid == UUID::EMPTY);
        CHECK(uuid.ToString() == "00000000-0000-0000-0000-000000000000");
    }

    SECTION("Constructor with data parts")
    {
        UUID uuid(0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321);
        CHECK(!uuid.Empty());
        CHECK(uuid.ToString() == "12345678-9abc-def0-0fed-cba987654321");
    }

    SECTION("Constructor from string")
    {
        String uuidStr = "550e8400-e29b-41d4-a716-446655440000";
        UUID uuid(uuidStr);
        CHECK(uuid.ToString() == uuidStr);
    }
}

TEST_CASE("UUID::Comparison", "[UUID]")
{
    UUID uuid1(1, 2, 3, 4);
    UUID uuid2(1, 2, 3, 4);
    UUID uuid3(1, 2, 3, 5);

    CHECK(uuid1 == uuid2);
    CHECK(uuid1 != uuid3);
    CHECK(uuid1 < uuid3);
    CHECK(!(uuid3 < uuid1));
}

TEST_CASE("UUID::Generator", "[UUID]")
{
    UUID uuid1 = UuidGenerator::Generate();
    UUID uuid2 = UuidGenerator::Generate();

    CHECK(!uuid1.Empty());
    CHECK(!uuid2.Empty());
    CHECK(uuid1 != uuid2);
}
