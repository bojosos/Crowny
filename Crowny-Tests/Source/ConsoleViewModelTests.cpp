#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/ConsoleViewModel.h"

using namespace Crowny;

namespace
{
    ConsoleBuffer::Message MakeMessage(ConsoleBuffer::Message::Level level, uint32_t repeatCount = 1u)
    {
        ConsoleBuffer::Message message;
        message.LogLevel = level;
        message.RepeatCount = repeatCount;
        return message;
    }
} // namespace

TEST_CASE("Console view model retains severity summaries", "[Editor][Console]")
{
    const Vector<ConsoleBuffer::Message> messages = {
        MakeMessage(ConsoleBuffer::Message::Level::Info, 3u),
        MakeMessage(ConsoleBuffer::Message::Level::Error, 2u),
        MakeMessage(ConsoleBuffer::Message::Level::Critical),
    };
    const Vector<uint32_t> visibleIndices = { 0u, 2u };
    ConsoleViewModel model;

    model.UpdateMessages(messages, visibleIndices, true);

    const ConsoleViewModel::Summary& summary = model.GetSummary();
    CHECK(summary.Levels[0].Count == 3u);
    CHECK(summary.Levels[1].Count == 0u);
    CHECK(summary.Levels[2].Count == 2u);
    CHECK(summary.Levels[3].Count == 1u);
    CHECK(StringView(summary.Levels[0].Label.data()) == "Info  3");
    CHECK(StringView(summary.Levels[2].Label.data()) == "Error  2");
    CHECK(summary.ShownCount == 4u);

    model.UpdateMessages(messages, visibleIndices, false);
    CHECK(model.GetSummary().Levels[0].Count == 1u);
    CHECK(model.GetSummary().ShownCount == 2u);
}

TEST_CASE("Console view model retains selected callstack labels", "[Editor][Console]")
{
    ConsoleBuffer::Message message;
    message.Sequence = 42u;
    message.Callstack = {
        { "Player::Update()", "Scripts/Player.cs", 42u },
        { "Game::Tick()", "Scripts/Game.cs", 7u },
    };
    ConsoleViewModel model;

    model.UpdateSelection(&message);
    REQUIRE(model.GetCallstackSourceLabels().size() == 2u);
    CHECK(model.GetCallstackSourceLabels()[0] == "Scripts/Player.cs:42");
    CHECK(model.GetCallstackSourceLabels()[1] == "Scripts/Game.cs:7");

    model.UpdateSelection(nullptr);
    CHECK(model.GetCallstackSourceLabels().empty());
}

TEST_CASE("Console view model reads allocate nothing after warm-up", "[Editor][Console][Memory][Frame]")
{
    Vector<ConsoleBuffer::Message> messages = {
        MakeMessage(ConsoleBuffer::Message::Level::Info, 3u),
        MakeMessage(ConsoleBuffer::Message::Level::Warn),
    };
    messages[0].Sequence = 42u;
    messages[0].Callstack = { { "Player::Update()", "Scripts/Player.cs", 42u } };
    const Vector<uint32_t> visibleIndices = { 0u, 1u };
    ConsoleViewModel model;
    model.UpdateMessages(messages, visibleIndices, true);
    model.UpdateSelection(&messages[0]);

    const uint64_t expectedPerFrame = 4u + StringView("Scripts/Player.cs:42").size();
    uint64_t checksum = 0u;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120u; frame++)
    {
        model.UpdateSelection(&messages[0]);
        checksum += model.GetSummary().ShownCount;
        checksum += model.GetCallstackSourceLabels()[0].size();
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(checksum == expectedPerFrame * 120u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
