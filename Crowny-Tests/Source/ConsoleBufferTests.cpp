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
    CHECK(messages[0].GroupSequence == messages[0].Sequence);
    CHECK(messages[1].GroupSequence == messages[0].GroupSequence);
    CHECK(messages[0].SearchText.find("could not load asset") != String::npos);
    CHECK(messages[0].SourceSearchText.find("scripts/player.cs") != String::npos);

    buffer.Collapse();
    CHECK(buffer.GetRevision() > normalRevision);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].RepeatCount == 2);
    CHECK(messages[0].GroupSequence == messages[0].Sequence);
    const uint64_t collapsedId = messages[0].Sequence;

    buffer.Sort(0, false);
    buffer.AddMessage(ConsoleBuffer::Message::Level::Warn, "One warning");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Error, "Could not load asset", callstack);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 2);
    CHECK(messages.front().Sequence == collapsedId);
    CHECK(messages.front().RepeatCount == 3);
}

TEST_CASE("Console search queries support fields, phrases, and exclusions", "[Common][Console][Search]")
{
    ConsoleBuffer buffer;
    const ConsoleBuffer::CallstackBuffer callstack = { { "PlayerController::Update()", "Scripts/Player Controller.cs", 42 } };
    buffer.AddMessage(ConsoleBuffer::Message::Level::Error, "Could not load asset", callstack);

    Vector<ConsoleBuffer::Message> messages;
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 1);
    const ConsoleBuffer::Message& message = messages.front();

    ConsoleBuffer::SearchQuery query;
    query.Set(R"(TEXT:"load asset" source:"player controller.cs" LEVEL:err -source:generated)");
    CHECK(query.Matches(message));

    query.Set(R"("could not load" -level:warn)");
    CHECK(query.Matches(message));

    query.Set(R"(text:"load asset" -source:scripts)");
    CHECK_FALSE(query.Matches(message));

    query.Set("time:" + message.TimestampText);
    CHECK(query.Matches(message));

    query.Set("unknown:load");
    CHECK_FALSE(query.Matches(message));

    query.Set({});
    CHECK(query.Empty());
    CHECK(query.Matches(message));

    ConsoleBuffer::Message escapedMessage;
    escapedMessage.SearchText = R"(quoted "asset" at c:\temp)";
    query.Set(R"(text:"quoted \"asset\" at c:\\temp")");
    CHECK(query.Matches(escapedMessage));
}

TEST_CASE("Collapsed console ordering uses stable sequence tie breakers", "[Common][Console][Ordering]")
{
    ConsoleBuffer buffer;
    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Alpha");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Beta");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Alpha");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Gamma");
    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Beta");

    Vector<ConsoleBuffer::Message> messages;
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 5);
    for (size_t index = 1; index < messages.size(); index++)
        CHECK(messages[index - 1].Sequence < messages[index].Sequence);
    for (const ConsoleBuffer::Message& message : messages)
        CHECK(message.TimestampText.size() == 8);

    buffer.Collapse();
    buffer.Sort(0, false);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 3);
    CHECK(messages[0].MessageText == "Beta");
    CHECK(messages[0].RepeatCount == 2);
    CHECK(messages[1].MessageText == "Alpha");
    CHECK(messages[1].RepeatCount == 2);
    CHECK(messages[2].MessageText == "Gamma");
    CHECK(messages[2].RepeatCount == 1);

    buffer.AddMessage(ConsoleBuffer::Message::Level::Info, "Alpha");
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 3);
    CHECK(messages.front().MessageText == "Alpha");
    CHECK(messages.front().RepeatCount == 3);

    buffer.Uncollapse();
    buffer.Sort(0, false);
    buffer.CopyBuffer(messages);
    REQUIRE(messages.size() == 6);
    for (size_t index = 1; index < messages.size(); index++)
        CHECK(messages[index - 1].Sequence > messages[index].Sequence);
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
