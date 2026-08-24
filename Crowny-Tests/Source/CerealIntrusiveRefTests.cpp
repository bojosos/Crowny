#include <catch2/catch_test_macros.hpp>

#include "Crowny/Common/DataStream.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include <cereal/types/base_class.hpp>

using namespace Crowny;

namespace IntrusiveRefTests
{
    class Node : public RefCounted
    {
    public:
        Node() { ++LiveInstances; }
        ~Node() override { --LiveInstances; }

        template <class Archive> void Serialize(Archive& archive) { archive(Value, Next); }

        int Value = 0;
        Ref<Node> Next;
        static inline int LiveInstances = 0;
    };

    class Base : public RefCounted
    {
    public:
        Base() { ++LiveInstances; }
        ~Base() override { --LiveInstances; }

        virtual int GetDerivedValue() const = 0;

        template <class Archive> void Serialize(Archive& archive) { archive(BaseValue); }

        int BaseValue = 0;
        static inline int LiveInstances = 0;
    };

    class Derived final : public Base
    {
    public:
        int GetDerivedValue() const override { return DerivedValue; }

        template <class Archive> void Serialize(Archive& archive) { archive(cereal::base_class<Base>(this), DerivedValue, Link); }

        int DerivedValue = 0;
        Ref<Base> Link;
    };
} // namespace IntrusiveRefTests

CEREAL_REGISTER_TYPE_WITH_NAME(IntrusiveRefTests::Derived, "IntrusiveRefTests.Derived")
CEREAL_REGISTER_POLYMORPHIC_RELATION(IntrusiveRefTests::Base, IntrusiveRefTests::Derived)

TEST_CASE("IntrusiveRef preserves nulls, identity, and cycles", "[Serialization][IntrusiveRef]")
{
    REQUIRE(IntrusiveRefTests::Node::LiveInstances == 0);
    Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();

    Ref<IntrusiveRefTests::Node> first = CreateRef<IntrusiveRefTests::Node>();
    Ref<IntrusiveRefTests::Node> second = CreateRef<IntrusiveRefTests::Node>();
    first->Value = 11;
    second->Value = 22;
    first->Next = second;
    second->Next = first;
    Ref<IntrusiveRefTests::Node> alias = first;
    Ref<IntrusiveRefTests::Node> null;

    {
        BinaryDataStreamOutputArchive archive(stream);
        archive(first, alias, null);
    }

    first->Next = nullptr;
    second->Next = nullptr;
    first = nullptr;
    second = nullptr;
    alias = nullptr;
    REQUIRE(IntrusiveRefTests::Node::LiveInstances == 0);

    stream->Seek(0);
    Ref<IntrusiveRefTests::Node> loaded;
    Ref<IntrusiveRefTests::Node> loadedAlias;
    Ref<IntrusiveRefTests::Node> loadedNull;
    {
        BinaryDataStreamInputArchive archive(stream);
        archive(loaded, loadedAlias, loadedNull);
    }

    REQUIRE(loaded);
    REQUIRE(loaded->Next);
    CHECK(loaded.Get() == loadedAlias.Get());
    CHECK(loaded->Next->Next.Get() == loaded.Get());
    CHECK(loaded->Value == 11);
    CHECK(loaded->Next->Value == 22);
    CHECK_FALSE(loadedNull);

    loaded->Next->Next = nullptr;
    loaded->Next = nullptr;
    loadedAlias = nullptr;
    loaded = nullptr;
    CHECK(IntrusiveRefTests::Node::LiveInstances == 0);
}

TEST_CASE("IntrusiveRef restores registered polymorphic objects", "[Serialization][IntrusiveRef]")
{
    REQUIRE(IntrusiveRefTests::Base::LiveInstances == 0);
    Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();
    Ref<IntrusiveRefTests::Derived> derived = CreateRef<IntrusiveRefTests::Derived>();
    derived->BaseValue = 31;
    derived->DerivedValue = 47;
    Ref<IntrusiveRefTests::Base> root = derived;
    derived->Link = root;

    {
        BinaryDataStreamOutputArchive archive(stream);
        archive(root);
    }

    derived->Link = nullptr;
    derived = nullptr;
    root = nullptr;
    REQUIRE(IntrusiveRefTests::Base::LiveInstances == 0);

    stream->Seek(0);
    Ref<IntrusiveRefTests::Base> loaded;
    {
        BinaryDataStreamInputArchive archive(stream);
        archive(loaded);
    }

    REQUIRE(loaded);
    Ref<IntrusiveRefTests::Derived> loadedDerived = DynamicRefCast<IntrusiveRefTests::Derived>(loaded);
    REQUIRE(loadedDerived);
    CHECK(loadedDerived->BaseValue == 31);
    CHECK(loadedDerived->DerivedValue == 47);
    CHECK(loadedDerived->Link.Get() == loaded.Get());

    loadedDerived->Link = nullptr;
    loadedDerived = nullptr;
    loaded = nullptr;
    CHECK(IntrusiveRefTests::Base::LiveInstances == 0);
}
