#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Application/Application.h"

using namespace Crowny;

// Minimal fixture for scene serialization tests.
class SerializationTestFixture {
public:
    SerializationTestFixture() {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc desc;
            desc.Name = "Test";
            desc.Headless = true;
            desc.WorkingDirectory = "C:\\\\dev\\\\Crowny";
            Application::StartUp(desc);
        }
    }

    ~SerializationTestFixture() {}
};

TEST_CASE("Complex Scene Serialization", "[Serialization]") {
    SerializationTestFixture fixture;
    
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("ComplexScene");

    // Create a hierarchy:
    // Parent (10, 0, 0)
    //   +- Child1 (5, 0, 0) -> World (15, 0, 0)
    //   +- Child2 (0, 10, 0) -> World (10, 10, 0)
    //      +- GrandChild (0, 0, 5) -> World (10, 10, 5)

    Entity parent = scene->CreateEntity("Parent");
    parent.SetPosition({10.0f, 0.0f, 0.0f});

    Entity child1 = scene->CreateEntity("Child1");
    child1.SetParent(parent);
    child1.SetPosition({5.0f, 0.0f, 0.0f});

    auto& asc = child1.AddComponent<AudioSourceComponent>();
    asc.SetVolume(0.5f);
    asc.SetPitch(1.5f);
    asc.SetLooping(true);

    Entity child2 = scene->CreateEntity("Child2");
    child2.SetParent(parent);
    child2.SetPosition({0.0f, 10.0f, 0.0f});

    Entity grandChild = scene->CreateEntity("GrandChild");
    grandChild.SetParent(child2);
    grandChild.SetPosition({0.0f, 0.0f, 5.0f});

    Path testPath = "complex_scene.yaml";

    SECTION("Serialize and Deserialize Hierarchy and Components") {
        {
            SceneSerializer serializer(scene);
            serializer.Serialize(testPath);
        }

        Ref<Scene> deserializedScene = CreateRef<Scene>(false);
        {
            SceneSerializer serializer(deserializedScene);
            serializer.Deserialize(testPath);
        }

        Entity dParent = deserializedScene->FindEntityByName("Parent");
        Entity dChild1 = deserializedScene->FindEntityByName("Child1");
        Entity dChild2 = deserializedScene->FindEntityByName("Child2");
        Entity dGrandChild = deserializedScene->FindEntityByName("GrandChild");

        REQUIRE(dParent);
        REQUIRE(dChild1);
        REQUIRE(dChild2);
        REQUIRE(dGrandChild);

        CHECK(dChild1.GetParent() == dParent);
        CHECK(dChild2.GetParent() == dParent);
        CHECK(dGrandChild.GetParent() == dChild2);

        CHECK(dParent.GetWorldPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
        CHECK(dChild1.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
        CHECK(dChild2.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 0.0f));
        CHECK(dGrandChild.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 5.0f));

        REQUIRE(dChild1.HasComponent<AudioSourceComponent>());
        auto& dAsc = dChild1.GetComponent<AudioSourceComponent>();
        CHECK(dAsc.GetVolume() == 0.5f);
        CHECK(dAsc.GetPitch() == 1.5f);
        CHECK(dAsc.GetLooping() == true);
    }
}
