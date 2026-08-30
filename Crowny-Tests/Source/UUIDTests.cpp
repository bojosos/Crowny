#include "Crowny/Common/Uuid.h"
#include "Crowny/Memory/AllocationCounter.h"

#include <catch2/catch_test_macros.hpp>

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
        CHECK(!uuid.Empty());
    }

    SECTION("Invalid string constructor results in empty UUID")
    {
        UUID uuid1("not-a-uuid");
        CHECK(uuid1.Empty());

        UUID uuid2("550e8400-e29b-41d4-a716"); // Too short
        CHECK(uuid2.Empty());

        UUID uuid3("550e8400-e29b-41d4-a716-44665544000z"); // Invalid hex
        CHECK(uuid3.Empty());

        UUID uuid4("550e8400e29b-41d4-a716-446655440000"); // Missing first hyphen
        CHECK(uuid4.Empty());

        UUID uuid5("550e8400-e29b041d4-a716-446655440000"); // Hyphen in wrong position
        CHECK(uuid5.Empty());
    }

    SECTION("Uppercase hex input")
    {
        UUID lower("550e8400-e29b-41d4-a716-446655440000");
        UUID upper("550E8400-E29B-41D4-A716-446655440000");
        CHECK(lower == upper);
        CHECK(!upper.Empty());
    }

    SECTION("Round-trip: generated UUID survives string conversion")
    {
        UUID original = UuidGenerator::Generate();
        String str = original.ToString();
        UUID reconstructed(str);
        CHECK(original == reconstructed);
        CHECK(reconstructed.ToString() == str);
    }
}

TEST_CASE("UUID::Comparison", "[UUID]")
{
    UUID uuid1(1, 2, 3, 4);
    UUID uuid2(1, 2, 3, 4);
    UUID uuid3(1, 2, 3, 5);

    CHECK(uuid1 == uuid2);
    CHECK(uuid1 != uuid3);

    // Strict weak ordering for containers
    if (uuid1 < uuid3)
    {
        CHECK(!(uuid3 < uuid1));
    }
    else
    {
        CHECK(uuid3 < uuid1);
        CHECK(!(uuid1 < uuid3));
    }
}

TEST_CASE("UUID::Generator", "[UUID]")
{
    SECTION("Uniqueness")
    {
        UUID uuid1 = UuidGenerator::Generate();
        UUID uuid2 = UuidGenerator::Generate();

        CHECK(!uuid1.Empty());
        CHECK(!uuid2.Empty());
        CHECK(uuid1 != uuid2);
    }

    SECTION("Version 4 Format")
    {
        // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        // where y is one of 8, 9, a, or b.
        UUID uuid = UuidGenerator::Generate();
        String s = uuid.ToString();

        // Version digit (the 13th hex digit, or 14th char including hyphen)
        CHECK(s[14] == '4');

        // Variant digit (the 17th hex digit, or 19th char)
        char variant = s[19];
        CHECK((variant == '8' || variant == '9' || variant == 'a' || variant == 'b'));
    }
}

TEST_CASE("UUID::Hashing", "[UUID]")
{
    UUID uuid1 = UuidGenerator::Generate();
    UUID uuid2 = uuid1;
    UUID uuid3 = UuidGenerator::Generate();

    std::hash<UUID> hasher;
    CHECK(hasher(uuid1) == hasher(uuid2));
    if (uuid1 != uuid3)
    {
        CHECK(hasher(uuid1) != hasher(uuid3));
    }
}

TEST_CASE("UUID fixed-buffer formatting is canonical and allocation-free", "[UUID][Memory][Frame]")
{
    const UUID uuid(0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321);
    constexpr StringView expected = "12345678-9abc-def0-0fed-cba987654321";

    const UUID::TextBuffer text = uuid.ToTextBuffer();
    CHECK(StringView(text.data(), UUID::TextLength) == expected);
    CHECK(text[UUID::TextLength] == '\0');

    constexpr size_t iterationCounts[] = { 1u, 1000u, 10000u };
    for (const size_t iterationCount : iterationCounts)
    {
        size_t checksum = 0;
        bool canonical = true;
        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        for (size_t iteration = 0; iteration < iterationCount; ++iteration)
        {
            const UUID::TextBuffer formatted = uuid.ToTextBuffer();
            canonical &= StringView(formatted.data(), UUID::TextLength) == expected;
            canonical &= formatted[UUID::TextLength] == '\0';
            checksum += static_cast<uint8_t>(formatted[iteration % UUID::TextLength]);
        }
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

        INFO("Iterations: " << iterationCount);
        CHECK(canonical);
        CHECK(checksum != 0u);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}
