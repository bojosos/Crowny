#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include "cwpch.h"
#include "Crowny/Threading/TaskSystem.h"

#include <atomic>
#include <thread>
#include <chrono>

using namespace Crowny;

TEST_CASE("TaskSystem", "[Threading]")
{
    // TaskSystem is a singleton that gets created in the constructor and stored in s_Instance
    TaskSystem system;

    SECTION("Basic Task Execution")
    {
        std::atomic<bool> executed = false;
        auto task = Task::Create("TestTask", [&executed]() {
            executed = true;
        });

        system.Submit(task);
        task->Wait();

        CHECK(executed.load());
        CHECK(task->IsComplete());
    }

    SECTION("Task Dependencies")
    {
        std::atomic<int> sequence = 0;
        
        auto taskA = Task::Create("TaskA", [&sequence]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            sequence = 1;
        });

        auto taskB = Task::Create("TaskB", [&sequence]() {
            // This should only run after TaskA
            if (sequence == 1) sequence = 2;
        }, TaskPriority::Normal, taskA);

        system.Submit(taskA);
        system.Submit(taskB);

        taskB->Wait();
        CHECK(sequence == 2);
    }

    SECTION("Task Groups")
    {
        const uint32_t count = 10;
        std::atomic<uint32_t> counter = 0;

        auto group = TaskGroup::Create("TestGroup", [&counter](uint32_t index) {
            counter.fetch_add(1);
        }, count);

        system.Submit(group);
        group->Wait();

        CHECK(counter == count);
        CHECK(group->IsComplete());
    }

    SECTION("Task Cancellation")
    {
        // We submit a task that hasn't started yet and cancel it.
        // To ensure it hasn't started, we fill the queue with some dummy work first.
        
        for (uint32_t i = 0; i < system.GetWorkerCount() * 2; ++i)
        {
            system.Submit(Task::Create("Dummy", []() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }));
        }

        std::atomic<bool> executed = false;
        auto task = Task::Create("CanceledTask", [&executed]() {
            executed = true;
        });

        task->Cancel();
        system.Submit(task);
        task->Wait();

        CHECK_FALSE(executed.load());
        CHECK(task->IsCanceled());
    }

    SECTION("Priority Handling")
    {
        // This is harder to test deterministically without many tasks,
        // but we can at least verify that high priority tasks are submitted and run.
        std::atomic<uint32_t> highCount = 0;
        std::atomic<uint32_t> lowCount = 0;

        auto highTask = Task::Create("High", [&highCount]() { highCount++; }, TaskPriority::High);
        auto lowTask = Task::Create("Low", [&lowCount]() { lowCount++; }, TaskPriority::Low);

        system.Submit(lowTask);
        system.Submit(highTask);

        highTask->Wait();
        lowTask->Wait();

        CHECK(highCount == 1);
        CHECK(lowCount == 1);
    }
}
