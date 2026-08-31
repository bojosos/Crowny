#include "cwpch.h"

#include "Crowny/Window/WindowSystemState.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Window-system shutdown waits for every native window", "[Window][Lifetime]")
{
    Detail::WindowSystemState state;
    state.MarkInitialized();
    REQUIRE(state.RegisterWindow());
    REQUIRE(state.RegisterWindow());

    CHECK(state.RequestShutdown() == Detail::WindowSystemAction::None);
    CHECK(state.IsInitialized());
    CHECK(state.IsShutdownPending());
    CHECK(state.GetLiveWindowCount() == 2);

    CHECK(state.UnregisterWindow() == Detail::WindowSystemAction::None);
    CHECK(state.IsInitialized());
    CHECK(state.GetLiveWindowCount() == 1);

    CHECK(state.UnregisterWindow() == Detail::WindowSystemAction::TerminateBackend);
    CHECK_FALSE(state.IsInitialized());
    CHECK_FALSE(state.IsShutdownPending());
    CHECK(state.GetLiveWindowCount() == 0);
}

TEST_CASE("Window-system shutdown without live windows is immediate", "[Window][Lifetime]")
{
    Detail::WindowSystemState state;
    state.MarkInitialized();

    CHECK(state.RequestShutdown() == Detail::WindowSystemAction::TerminateBackend);
    CHECK_FALSE(state.IsInitialized());
    CHECK(state.UnregisterWindow() == Detail::WindowSystemAction::None);
}

TEST_CASE("Reusing an initialized window system cancels deferred shutdown", "[Window][Lifetime]")
{
    Detail::WindowSystemState state;
    state.MarkInitialized();
    REQUIRE(state.RegisterWindow());
    REQUIRE(state.RequestShutdown() == Detail::WindowSystemAction::None);

    state.CancelPendingShutdown();
    CHECK_FALSE(state.IsShutdownPending());
    CHECK(state.UnregisterWindow() == Detail::WindowSystemAction::None);
    CHECK(state.IsInitialized());
    CHECK(state.RequestShutdown() == Detail::WindowSystemAction::TerminateBackend);
}

TEST_CASE("Window registration requires an initialized backend", "[Window][Lifetime]")
{
    Detail::WindowSystemState state;
    CHECK_FALSE(state.RegisterWindow());
    CHECK(state.GetLiveWindowCount() == 0);

    state.MarkInitialized();
    state.MarkInitializationFailed();
    CHECK_FALSE(state.RegisterWindow());
    CHECK_FALSE(state.IsInitialized());
}
