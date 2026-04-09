#include <catch2/catch_test_macros.hpp>
#include "Crowny/Utils/SmallVector.h"
#include <string>

using namespace Crowny;

struct TestType
{
    static int ConstructorCount;
    static int DestructorCount;

    std::string Value;

    TestType() : Value("Default") { ConstructorCount++; }
    TestType(const std::string& v) : Value(v) { ConstructorCount++; }
    TestType(const TestType& other) : Value(other.Value) { ConstructorCount++; }
    TestType(TestType&& other) noexcept : Value(std::move(other.Value)) { ConstructorCount++; }
    ~TestType() { DestructorCount++; }

    TestType& operator=(const TestType& other) { Value = other.Value; return *this; }
    TestType& operator=(TestType&& other) noexcept { Value = std::move(other.Value); return *this; }

    bool operator==(const TestType& other) const { return Value == other.Value; }
};

int TestType::ConstructorCount = 0;
int TestType::DestructorCount = 0;

TEST_CASE("SmallVector::Basic", "[SmallVector]")
{
    SmallVector<int, 4> v;
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 4);
    CHECK(v.empty());

    SECTION("Push back within static capacity")
    {
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        v.push_back(4);
        CHECK(v.size() == 4);
        CHECK(v.capacity() == 4);
        CHECK(v[0] == 1);
        CHECK(v[3] == 4);
    }

    SECTION("Growth beyond static capacity")
    {
        for (int i = 0; i < 10; ++i)
            v.push_back(i);
        CHECK(v.size() == 10);
        CHECK(v.capacity() >= 10);
        CHECK(v[9] == 9);
    }

    SECTION("Initializer list")
    {
        SmallVector<int, 2> v2 = { 1, 2, 3 };
        CHECK(v2.size() == 3);
        CHECK(v2.capacity() >= 3);
        CHECK(v2[0] == 1);
        CHECK(v2[2] == 3);
    }
}

TEST_CASE("SmallVector::ComplexTypes", "[SmallVector]")
{
    TestType::ConstructorCount = 0;
    TestType::DestructorCount = 0;

    {
        SmallVector<TestType, 2> v;
        v.push_back(TestType("One"));
        v.emplace_back("Two");
        v.push_back(TestType("Three")); // Causes growth

        CHECK(v.size() == 3);
        CHECK(v[0].Value == "One");
        CHECK(v[1].Value == "Two");
        CHECK(v[2].Value == "Three");
    }

    CHECK(TestType::ConstructorCount == TestType::DestructorCount);
}

TEST_CASE("SmallVector::AssignmentAndMove", "[SmallVector]")
{
    SmallVector<int, 4> v1 = { 1, 2, 3 };
    
    SECTION("Copy assignment")
    {
        SmallVector<int, 4> v2;
        v2 = v1;
        CHECK(v2.size() == 3);
        CHECK(v2 == v1);
        v2.push_back(4);
        CHECK(v2.size() == 4);
        CHECK(v1.size() == 3);
    }

    SECTION("Move constructor - from static to static")
    {
        SmallVector<int, 4> v2 = std::move(v1);
        CHECK(v2.size() == 3);
        CHECK(v2[0] == 1);
        // v1 should be empty but still usable
        CHECK(v1.empty());
    }

    SECTION("Move constructor - from heap to heap")
    {
        SmallVector<int, 2> v_heap = { 1, 2, 3, 4, 5 };
        uint32_t old_cap = v_heap.capacity();
        const int* old_data = v_heap.data();

        SmallVector<int, 2> v2 = std::move(v_heap);
        CHECK(v2.size() == 5);
        CHECK(v2.capacity() == old_cap);
        CHECK(v2.data() == old_data);
        CHECK(v_heap.empty());
    }

    SECTION("Move assignment - from static to static")
    {
        SmallVector<int, 4> v2 = { 10, 20 };
        v2 = std::move(v1);
        CHECK(v2.size() == 3);
        CHECK(v2[0] == 1);
        CHECK(v2[1] == 2);
        CHECK(v2[2] == 3);
        CHECK(v1.empty());
    }

    SECTION("Move assignment - from heap to heap")
    {
        SmallVector<int, 2> v_src = { 1, 2, 3, 4, 5 };
        SmallVector<int, 2> v_dst = { 10, 20, 30 };
        const int* old_data = v_src.data();

        v_dst = std::move(v_src);
        CHECK(v_dst.size() == 5);
        CHECK(v_dst.data() == old_data);
        CHECK(v_dst[0] == 1);
        CHECK(v_dst[4] == 5);
        CHECK(v_src.empty());
    }

    SECTION("Self copy-assignment is safe")
    {
        SmallVector<int, 4> v_self = { 1, 2, 3 };
        v_self = v_self;
        CHECK(v_self.size() == 3);
        CHECK(v_self[0] == 1);
        CHECK(v_self[1] == 2);
        CHECK(v_self[2] == 3);
    }
}

TEST_CASE("SmallVector::Operations", "[SmallVector]")
{
    SmallVector<int, 4> v = { 1, 2, 3, 4 };

    SECTION("Erase middle")
    {
        auto it = v.erase(v.begin() + 1);
        CHECK(v.size() == 3);
        CHECK(*it == 3);
        CHECK(v[0] == 1);
        CHECK(v[1] == 3);
        CHECK(v[2] == 4);
    }

    SECTION("Erase last")
    {
        v.erase(v.end() - 1);
        CHECK(v.size() == 3);
        CHECK(v.back() == 3);
    }

    SECTION("Resize")
    {
        v.resize(2);
        CHECK(v.size() == 2);
        v.resize(5, 10);
        CHECK(v.size() == 5);
        CHECK(v[4] == 10);
    }

    SECTION("Clear")
    {
        v.clear();
        CHECK(v.empty());
        CHECK(v.size() == 0);
    }

    SECTION("Pop back")
    {
        TestType::ConstructorCount = 0;
        TestType::DestructorCount = 0;
        {
            SmallVector<TestType, 4> vt;
            vt.emplace_back("One");
            vt.emplace_back("Two");
            CHECK(vt.size() == 2);
            vt.pop_back();
            CHECK(vt.size() == 1);
            CHECK(vt.back().Value == "One");
            CHECK(TestType::DestructorCount == 1);
        }
        CHECK(TestType::DestructorCount == 2);
    }
}
