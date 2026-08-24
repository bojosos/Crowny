#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/Module.h"

using namespace Crowny;

class TestModule : public Module<TestModule>
{
public:
    TestModule(int val = 0) : value(val) {}
    int value = 0;
    bool startedUp = false;
    bool shutDown = false;

protected:
    void OnStartUp() override { startedUp = true; }
    void OnShutdown() override { shutDown = true; }
};

// Reset static state between test cases to avoid cross-test contamination
struct ModuleFixture
{
    ~ModuleFixture()
    {
        if (TestModule::IsStartedUp())
            TestModule::Shutdown();
    }
};

TEST_CASE_METHOD(ModuleFixture, "Module::StartUpShutdownLifecycle", "[Module]")
{
    SECTION("StartUp initializes module and calls OnStartUp")
    {
        CHECK_FALSE(TestModule::IsStartedUp());
        TestModule::StartUp();
        CHECK(TestModule::IsStartedUp());
        CHECK(TestModule::Get().startedUp);
        CHECK_FALSE(TestModule::Get().shutDown);
    }

    SECTION("Shutdown tears down module and calls OnShutdown")
    {
        TestModule::StartUp();
        TestModule& ref = TestModule::Get();
        CHECK(ref.startedUp);

        TestModule::Shutdown();
        CHECK_FALSE(TestModule::IsStartedUp());
    }
}

TEST_CASE_METHOD(ModuleFixture, "Module::GetAfterStartUp", "[Module]")
{
    TestModule::StartUp(42);
    TestModule& ref = TestModule::Get();
    CHECK(ref.value == 42);
    CHECK(ref.startedUp);
}

TEST_CASE_METHOD(ModuleFixture, "Module::DoubleShutdownIsIdempotent", "[Module]")
{
    TestModule::StartUp();
    CHECK(TestModule::IsStartedUp());

    TestModule::Shutdown();
    CHECK_FALSE(TestModule::IsStartedUp());
    CHECK(TestModule::TryGet() == nullptr);

    // Second shutdown should return early without crashing
    TestModule::Shutdown();
    CHECK_FALSE(TestModule::IsStartedUp());
    CHECK(TestModule::TryGet() == nullptr);
}

TEST_CASE_METHOD(ModuleFixture, "Module::RestartAfterShutdown", "[Module]")
{
    TestModule::StartUp(10);
    CHECK(TestModule::Get().value == 10);
    TestModule::Shutdown();
    CHECK_FALSE(TestModule::IsStartedUp());

    // Restart with a different value
    TestModule::StartUp(20);
    CHECK(TestModule::IsStartedUp());
    CHECK(TestModule::Get().value == 20);
    CHECK(TestModule::Get().startedUp);
}

TEST_CASE_METHOD(ModuleFixture, "Module::IsStartedUpReturnsCorrectValues", "[Module]")
{
    CHECK_FALSE(TestModule::IsStartedUp());
    TestModule::StartUp();
    CHECK(TestModule::IsStartedUp());
    TestModule::Shutdown();
    CHECK_FALSE(TestModule::IsStartedUp());
}

TEST_CASE_METHOD(ModuleFixture, "Module::StartUpWithExternalInstance", "[Module]")
{
    auto external = std::make_unique<TestModule>(99);
    TestModule::StartUp(std::move(external));

    CHECK(external == nullptr);
    CHECK(TestModule::IsStartedUp());
    CHECK(TestModule::Get().value == 99);
    CHECK(TestModule::Get().startedUp);

    TestModule::Shutdown();
    CHECK_FALSE(TestModule::IsStartedUp());
}
