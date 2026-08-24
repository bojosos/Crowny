#include "Crowny/Common/ConsoleBuffer.h"

#include <catch2/catch_test_macros.hpp>

#include <thread>

using namespace Crowny;

TEST_CASE("Console messages collapse without losing stable identity", "[Common][Console]")
{
    ConsoleBuffer buffer;
    const ConsoleBuffer::CallstackBuffer callstack = { { "Update()", "Scripts/Player.cs", 42 } };

    buffer.AddMessage(ConsoleBuffer::Message::Level::Error, "Could not load asset", callstack);
    buffer.AddMessage(ConsoleBuffer::Message::Level::Error, "Could not load asset", callstack);

    Vector<ConsoleBuffer::Message> messages;
    const uint64_t normalRevision = buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].Sequence < messages[1].Sequence);
    CHECK(messages[0].SearchText.find("could not load asset") != String::npos);
    CHECK(messages[0].SourceSearchText.find("scripts/player.cs") != String::npos);

    buffer.Collapse();
    CHECK(buffer.GetRevision() > normalRevision);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].RepeatCount == 2);
    const uint64_t collapsedId = messages[0].Sequence;

    buffer.Sort(0, false);
    buffer.AddMessage(ConsoleBuffer::Message::Level::Warn, "One warning");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Error, "Could not load asset", callstack);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 2);
    CHECK(messages.front().Sequence == collapsedId);
    CHECK(messages.front().RepeatCount == 3);
}

TEST_CASE("Console snapshots are safe while messages are produced concurrently", "[Common][Console][Threading]")
{
    ConsoleBuffer buffer;
    constexpr uint32_t threadCount = 4;
    constexpr uint32_t messagesPerThread = 100;
    Array<std::thread, threadCount> producers;

    for (uint32_t threadIndex = 0; threadIndex < threadCount; threadIndex++)
    {
        producers[threadIndex] = std::thread([&buffer, threadIndex]() {
            for (uint32_t messageIndex = 0; messageIndex < messagesPerThread; messageIndex++)
                buffer.AddMessage(ConsoleBuffer::Message::Level::Info,
                                  "worker " + std::to_string(threadIndex) + " message " + std::to_string(messageIndex));
        });
    }
    for (std::thread& producer : producers)
        producer.join();

    Vector<ConsoleBuffer::Message> messages;
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == threadCount * messagesPerThread);

    UnorderedSet<uint64_t> sequenceIds;
    for (const ConsoleBuffer::Message& message : messages)
        sequenceIds.insert(message.Sequence);
    CHECK(sequenceIds.size() == messages.size());
    CHECK_FALSE(buffer.HasNewMessages());
}
