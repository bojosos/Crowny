#include "cwpch.h"

#include "Crowny/Window/WindowSystemState.h"

#include <catch2/catch_test_macros.hpp>

#if defined(CW_WINDOWS)
#include <GLFW/glfw3.h>
#include <Windows.h>
#endif

using namespace Crowny;

#if defined(CW_WINDOWS)
TEST_CASE("GLFW polling ignores another library's active-window property", "[Window][GLFW][.ProcessIsolated]")
{
    REQUIRE(glfwInit() == GLFW_TRUE);
    struct Windows
    {
        GLFWwindow* Owned = nullptr;
        HWND Foreign = nullptr;
        ~Windows()
        {
            if (Foreign)
            {
                RemovePropW(Foreign, L"GLFW");
                DestroyWindow(Foreign);
            }
            if (Owned)
                glfwDestroyWindow(Owned);
            glfwTerminate();
        }
    } windows;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    windows.Owned = glfwCreateWindow(32, 32, "GLFW ownership test", nullptr, nullptr);
    REQUIRE(windows.Owned);
    windows.Foreign = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Foreign window ownership test", WS_POPUP | WS_VISIBLE, -32000, -32000, 1, 1,
                                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    REQUIRE(windows.Foreign);
    // Window properties can contain pointers owned by another GLFW instance or
    // process. The event loop must establish ownership before dereferencing one.
    REQUIRE(SetPropW(windows.Foreign, L"GLFW", reinterpret_cast<HANDLE>(uintptr_t(1))) != FALSE);
    SetActiveWindow(windows.Foreign);
    REQUIRE(GetActiveWindow() == windows.Foreign);
    glfwPollEvents();
    CHECK(IsWindow(windows.Foreign));
}
#endif

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
