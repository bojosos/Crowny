#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "Crowny/Threading/TaskSystem.h"
#include "cwpch.h"

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace Crowny;
using namespace std::chrono_literals;

namespace
{
    class TestGate
    {
    public:
        ~TestGate() { Open(); }

        void EnterAndWait()
        {
            std::unique_lock lock(m_Mutex);
            m_Entered++;
            m_Changed.notify_all();
            m_Changed.wait(lock, [this]() { return m_Open; });
        }

        bool WaitForEntered(uint32_t count)
        {
            std::unique_lock lock(m_Mutex);
            return m_Changed.wait_for(lock, 2s, [this, count]() { return m_Entered >= count; });
        }

        void Open()
        {
            std::lock_guard lock(m_Mutex);
            m_Open = true;
            m_Changed.notify_all();
        }

    private:
        std::mutex m_Mutex;
        std::condition_variable m_Changed;
        uint32_t m_Entered = 0;
        bool m_Open = false;
    };

    struct BlockingCopyState
    {
        std::atomic<bool> Enabled{ false };
        TestGate Gate;
    };

    class BlockingCopyWorker
    {
    public:
        explicit BlockingCopyWorker(std::shared_ptr<BlockingCopyState> state) : m_State(std::move(state)) {}

        BlockingCopyWorker(const BlockingCopyWorker& other) : m_State(other.m_State)
        {
            if (m_State->Enabled.load(std::memory_order_acquire))
                m_State->Gate.EnterAndWait();
        }

        BlockingCopyWorker(BlockingCopyWorker&&) noexcept = default;
        BlockingCopyWorker& operator=(const BlockingCopyWorker&) = default;
        BlockingCopyWorker& operator=(BlockingCopyWorker&&) noexcept = default;

        void operator()(uint32_t) const {}

    private:
        std::shared_ptr<BlockingCopyState> m_State;
    };

    bool WaitFor(const std::function<bool()>& predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;
            std::this_thread::yield();
        }
        return predicate();
    }
} // namespace

TEST_CASE("TaskSystem preserves the legacy task and group interface", "[Threading][TaskSystem]")
{
    TaskSystem system(2);
    std::atomic<uint32_t> taskCalls{ 0 };
    std::atomic<uint32_t> groupCalls{ 0 };
    std::atomic<bool> canceledCall{ false };

    Ref<Task> task = Task::Create("Legacy task", [&]() { taskCalls.fetch_add(1, std::memory_order_relaxed); });
    Ref<TaskGroup> group = TaskGroup::Create("Legacy group", [&](uint32_t) { groupCalls.fetch_add(1, std::memory_order_relaxed); }, 11);
    Ref<Task> canceled = Task::Create("Legacy canceled task", [&]() { canceledCall.store(true, std::memory_order_release); });
    canceled->Cancel();
    system.Submit(task);
    system.Submit(group);
    system.Submit(canceled);
    task->Wait();
    group->Wait();
    canceled->Wait();

    CHECK(taskCalls.load(std::memory_order_acquire) == 1);
    CHECK(groupCalls.load(std::memory_order_acquire) == 11);
    CHECK(task->GetStatus() == TaskStatus::Succeeded);
    CHECK(group->GetStatus() == TaskStatus::Succeeded);
    CHECK(canceled->GetStatus() == TaskStatus::Canceled);
    CHECK_FALSE(canceled->HasStarted());
    CHECK_FALSE(canceledCall.load(std::memory_order_acquire));
}

TEST_CASE("Queued dependencies do not block the only worker", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate gate;
    Ref<Task> blocker = system.Submit("Blocker", [&]() { gate.EnterAndWait(); });
    REQUIRE(gate.WaitForEntered(1));

    std::atomic<uint32_t> sequence{ 0 };
    TaskOptions prerequisiteOptions;
    prerequisiteOptions.Priority = TaskPriority::Low;
    Ref<Task> prerequisite = system.Submit("Prerequisite", [&]() { sequence.store(1, std::memory_order_release); }, prerequisiteOptions);

    TaskOptions dependentOptions;
    dependentOptions.Priority = TaskPriority::High;
    dependentOptions.Dependency = prerequisite;
    Ref<Task> dependent = system.Submit(
      "Dependent",
      [&]() {
          uint32_t expected = 1;
          sequence.compare_exchange_strong(expected, 2, std::memory_order_acq_rel);
      },
      dependentOptions);
    CHECK(prerequisite->GetStatus() == TaskStatus::Queued);
    CHECK(dependent->GetStatus() == TaskStatus::Waiting);

    gate.Open();
    blocker->Wait();
    REQUIRE(dependent->WaitFor(2s));
    CHECK(sequence.load(std::memory_order_acquire) == 2);
}

TEST_CASE("A worker can submit and wait for a child with one worker", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    std::atomic<bool> childRan{ false };
    Ref<Task> parent = system.Submit("Parent", [&]() {
        Ref<Task> child = system.Submit("Child", [&]() { childRan.store(true, std::memory_order_release); });
        child->Wait();
    });

    REQUIRE(parent->WaitFor(2s));
    CHECK(childRan.load(std::memory_order_acquire));
}

TEST_CASE("TaskSystem rejects dynamic mutual waits", "[Threading][TaskSystem]")
{
    TaskSystem system(2);
    TestGate startGate;
    Ref<Task> first;
    Ref<Task> second;
    first = Task::Create("First", [&]() {
        startGate.EnterAndWait();
        second->Wait();
    });
    second = Task::Create("Second", [&]() {
        startGate.EnterAndWait();
        first->Wait();
    });

    system.Submit(first);
    system.Submit(second);
    REQUIRE(startGate.WaitForEntered(2));
    startGate.Open();

    REQUIRE(WaitFor([&]() { return first->IsComplete() && second->IsComplete(); }));
    CHECK_THROWS_WITH(first->Wait(), "Task wait would create a cycle");
    CHECK_THROWS_WITH(second->Wait(), "Task wait would create a cycle");
    CHECK(first->GetStatus() == TaskStatus::Failed);
    CHECK(second->GetStatus() == TaskStatus::Failed);
}

TEST_CASE("Timed worker waits do not execute an unbounded blocker", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate bootstrapGate;
    TestGate blockerGate;
    TaskOptions lowOptions;
    lowOptions.Priority = TaskPriority::Low;
    Ref<Task> bootstrap = system.Submit("Bootstrap", [&]() { bootstrapGate.EnterAndWait(); }, lowOptions);
    REQUIRE(bootstrapGate.WaitForEntered(1));

    std::atomic<bool> timedOut{ false };
    std::atomic<int64_t> elapsedMilliseconds{ 0 };
    Ref<Task> target;
    TaskOptions highOptions;
    highOptions.Priority = TaskPriority::High;
    Ref<Task> waiter = system.Submit(
      "Timed waiter",
      [&]() {
          const auto start = std::chrono::steady_clock::now();
          timedOut.store(!target->WaitFor(25ms), std::memory_order_release);
          elapsedMilliseconds.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
                                    std::memory_order_release);
      },
      highOptions);
    Ref<Task> blocker = system.Submit("Unbounded blocker", [&]() { blockerGate.EnterAndWait(); }, highOptions);
    target = system.Submit("Timed target", []() {}, lowOptions);

    bootstrapGate.Open();
    const bool waiterFinishedBeforeBlockerRelease = waiter->WaitFor(250ms);
    blockerGate.Open();

    CHECK(waiterFinishedBeforeBlockerRelease);
    waiter->Wait();
    blocker->Wait();
    target->Wait();
    bootstrap->Wait();
    CHECK(timedOut.load(std::memory_order_acquire));
    CHECK(elapsedMilliseconds.load(std::memory_order_acquire) < 200);
}

TEST_CASE("Task failures reach waiters without killing workers", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    Ref<Task> failed = system.Submit("Failure", []() { throw std::runtime_error("task failure"); });

    CHECK_THROWS_WITH(failed->Wait(), "task failure");
    CHECK(failed->GetStatus() == TaskStatus::Failed);

    std::atomic<bool> workerSurvived{ false };
    Ref<Task> recovery = system.Submit("Recovery", [&]() { workerSurvived.store(true, std::memory_order_release); });
    recovery->Wait();
    CHECK(workerSurvived.load(std::memory_order_acquire));
}

TEST_CASE("Cancellation reports whether work started and propagates through dependencies", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate gate;
    Ref<Task> blocker = system.Submit("Blocker", [&]() { gate.EnterAndWait(); });
    REQUIRE(gate.WaitForEntered(1));

    std::atomic<bool> canceledBodyRan{ false };
    Ref<Task> canceled = system.Submit("Canceled", [&]() { canceledBodyRan.store(true, std::memory_order_release); });
    TaskOptions dependentOptions;
    dependentOptions.Dependency = canceled;
    Ref<Task> dependent = system.Submit("Canceled dependent", []() {}, dependentOptions);

    CHECK(canceled->Cancel());
    canceled->Wait();
    dependent->Wait();
    CHECK(canceled->GetStatus() == TaskStatus::Canceled);
    CHECK(dependent->GetStatus() == TaskStatus::Canceled);
    CHECK_FALSE(canceled->HasStarted());
    CHECK_FALSE(canceledBodyRan.load(std::memory_order_acquire));

    gate.Open();
    blocker->Wait();
}

TEST_CASE("TaskGroup cancellation is atomic with completion publication", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate workerGate;
    Ref<Task> blocker = system.Submit("Worker blocker", [&]() { workerGate.EnterAndWait(); });
    REQUIRE(workerGate.WaitForEntered(1));

    const std::shared_ptr<BlockingCopyState> copyState = std::make_shared<BlockingCopyState>();
    Ref<TaskGroup> group = TaskGroup::Create("Publication race", BlockingCopyWorker(copyState), 4);
    copyState->Enabled.store(true, std::memory_order_release);

    std::future<void> submission = std::async(std::launch::async, [&]() { system.Submit(group); });
    const bool copyBlocked = copyState->Gate.WaitForEntered(1);
    if (!copyBlocked)
        copyState->Gate.Open();
    REQUIRE(copyBlocked);
    std::atomic<bool> cancelStarted{ false };
    std::future<bool> cancellation = std::async(std::launch::async, [&]() {
        cancelStarted.store(true, std::memory_order_release);
        return group->Cancel();
    });
    const bool cancellationDispatched = WaitFor([&]() { return cancelStarted.load(std::memory_order_acquire); });
    if (!cancellationDispatched)
        copyState->Gate.Open();
    REQUIRE(cancellationDispatched);
    CHECK(cancellation.wait_for(20ms) == std::future_status::timeout);

    copyState->Gate.Open();
    submission.get();
    REQUIRE(cancellation.get());
    group->Wait();
    CHECK(group->GetStatus() == TaskStatus::Canceled);

    workerGate.Open();
    blocker->Wait();
}

TEST_CASE("Running tasks observe cooperative cancellation", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    std::atomic<bool> entered{ false };
    Ref<Task> task = system.Submit("Cancelable", [&](TaskContext& context) {
        entered.store(true, std::memory_order_release);
        while (!context.IsCancellationRequested())
            std::this_thread::yield();
    });

    REQUIRE(WaitFor([&]() { return entered.load(std::memory_order_acquire); }));
    CHECK(task->Cancel());
    task->Wait();
    CHECK(task->GetStatus() == TaskStatus::Canceled);
    CHECK(task->HasStarted());
}

TEST_CASE("Dependency failures suppress dependent work", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    Ref<Task> prerequisite = system.Submit("Failed prerequisite", []() { throw std::runtime_error("dependency failure"); });
    std::atomic<bool> dependentRan{ false };
    TaskOptions options;
    options.Dependency = prerequisite;
    Ref<Task> dependent = system.Submit("Dependent", [&]() { dependentRan.store(true, std::memory_order_release); }, options);

    CHECK_THROWS_WITH(dependent->Wait(), "dependency failure");
    CHECK(dependent->GetStatus() == TaskStatus::Failed);
    CHECK_FALSE(dependentRan.load(std::memory_order_acquire));
}

TEST_CASE("Task handles cannot cross scheduler generations", "[Threading][TaskSystem]")
{
    Ref<Task> oldTask;
    {
        TaskSystem oldSystem(1);
        oldTask = oldSystem.Submit("Old generation", []() {});
        oldTask->Wait();
        oldSystem.Drain();
    }

    TaskSystem newSystem(1);
    TaskOptions options;
    options.Dependency = oldTask;
    CHECK_THROWS_AS(newSystem.Submit("Wrong generation", []() {}, options), std::invalid_argument);
}

TEST_CASE("Drain seals submissions without orphaning accepted work", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate gate;
    Ref<Task> running = system.Submit("Running", [&]() { gate.EnterAndWait(); });
    REQUIRE(gate.WaitForEntered(1));

    std::future<void> drain = std::async(std::launch::async, [&]() { system.Drain(); });
    const bool sealed = WaitFor([&]() { return !system.IsAccepting(); });
    CHECK(sealed);
    bool rejected = false;
    try
    {
        system.Submit("Too late", []() {});
    }
    catch (const std::logic_error&)
    {
        rejected = true;
    }
    catch (...)
    {
    }
    CHECK(rejected);

    gate.Open();
    drain.get();
    running->Wait();
    CHECK(running->GetStatus() == TaskStatus::Succeeded);
}

TEST_CASE("CancelPendingAndDrain cancels queued and cooperative running tasks", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    std::atomic<bool> entered{ false };
    Ref<Task> running = system.Submit("Running", [&](TaskContext& context) {
        entered.store(true, std::memory_order_release);
        while (!context.IsCancellationRequested())
            std::this_thread::yield();
    });
    REQUIRE(WaitFor([&]() { return entered.load(std::memory_order_acquire); }));
    Ref<Task> queued = system.Submit("Queued", []() {});

    system.CancelPendingAndDrain();
    running->Wait();
    queued->Wait();
    CHECK(running->GetStatus() == TaskStatus::Canceled);
    CHECK(queued->GetStatus() == TaskStatus::Canceled);
}

TEST_CASE("ParallelFor remains canceled when shutdown rejects a runner submission", "[Threading][TaskSystem]")
{
    TaskSystem system(2);
    TestGate submitGate;
    std::atomic<bool> blockSubmit{ true };
    TaskSystemTestAccess::SetBeforeParallelRunnerSubmit(system, [&]() {
        if (blockSubmit.exchange(false, std::memory_order_acq_rel))
            submitGate.EnterAndWait();
    });

    ParallelForOptions options;
    options.GrainSize = 1;
    options.MaxConcurrency = 2;
    Ref<Task> task = system.ParallelFor("Cancel during runner submission", 32, [](uint32_t) {}, options);
    REQUIRE(submitGate.WaitForEntered(1));

    std::future<void> shutdown = std::async(std::launch::async, [&]() { system.CancelPendingAndDrain(); });
    const bool shutdownStarted = WaitFor([&]() { return !system.IsAccepting(); });
    if (!shutdownStarted)
        submitGate.Open();
    REQUIRE(shutdownStarted);
    submitGate.Open();
    shutdown.get();

    task->Wait();
    CHECK(task->GetStatus() == TaskStatus::Canceled);
}

TEST_CASE("Priority queues preserve FIFO order and an eight-four-one service cycle", "[Threading][TaskSystem]")
{
    TaskSystem system(1);
    TestGate gate;
    TaskOptions lowOptions;
    lowOptions.Priority = TaskPriority::Low;
    Ref<Task> blocker = system.Submit("Blocker", [&]() { gate.EnterAndWait(); }, lowOptions);
    REQUIRE(gate.WaitForEntered(1));

    Vector<Ref<Task>> tasks;
    Vector<TaskPriority> priorities;
    Array<Vector<uint32_t>, 3> perPriorityOrder;
    std::mutex orderMutex;
    TaskOptions highOptions;
    highOptions.Priority = TaskPriority::High;
    TaskOptions normalOptions;
    const auto submitTasks = [&](TaskPriority priority, uint32_t count, const TaskOptions& options) {
        for (uint32_t index = 0; index < count; index++)
        {
            tasks.push_back(system.Submit(
              "Priority task",
              [&, priority, index]() {
                  std::lock_guard lock(orderMutex);
                  priorities.push_back(priority);
                  perPriorityOrder[static_cast<size_t>(priority)].push_back(index);
              },
              options));
        }
    };
    submitTasks(TaskPriority::High, 16, highOptions);
    submitTasks(TaskPriority::Normal, 8, normalOptions);
    submitTasks(TaskPriority::Low, 2, lowOptions);

    gate.Open();
    for (const Ref<Task>& task : tasks)
        task->Wait();
    blocker->Wait();

    const Array<TaskPriority, 13> expectedCycle = { TaskPriority::High,   TaskPriority::High,   TaskPriority::High,   TaskPriority::High,
                                                    TaskPriority::High,   TaskPriority::High,   TaskPriority::High,   TaskPriority::High,
                                                    TaskPriority::Normal, TaskPriority::Normal, TaskPriority::Normal, TaskPriority::Normal,
                                                    TaskPriority::Low };
    REQUIRE(priorities.size() == expectedCycle.size() * 2);
    for (size_t index = 0; index < priorities.size(); index++)
        CHECK(priorities[index] == expectedCycle[index % expectedCycle.size()]);
    for (const Vector<uint32_t>& order : perPriorityOrder)
    {
        for (uint32_t index = 0; index < order.size(); index++)
            CHECK(order[index] == index);
    }
}

TEST_CASE("ParallelFor covers every index exactly once", "[Threading][TaskSystem]")
{
    TaskSystem system(4);
    std::array<std::atomic<uint32_t>, 37> visits{};
    ParallelForOptions options;
    options.GrainSize = 4;
    options.MaxConcurrency = 3;
    Ref<Task> task = system.ParallelFor(
      "Coverage", static_cast<uint32_t>(visits.size()), [&](uint32_t index) { visits[index].fetch_add(1, std::memory_order_relaxed); }, options);
    task->Wait();

    for (const std::atomic<uint32_t>& count : visits)
        CHECK(count.load(std::memory_order_acquire) == 1);

    std::atomic<bool> emptyBodyRan{ false };
    Ref<Task> empty = system.ParallelFor("Empty", 0, [&](uint32_t) { emptyBodyRan.store(true, std::memory_order_release); });
    empty->Wait();
    CHECK(empty->GetStatus() == TaskStatus::Succeeded);
    CHECK_FALSE(emptyBodyRan.load(std::memory_order_acquire));
}

TEST_CASE("ParallelFor obeys its concurrency bound", "[Threading][TaskSystem]")
{
    TaskSystem system(4);
    TestGate gate;
    std::atomic<uint32_t> active{ 0 };
    std::atomic<uint32_t> maximumActive{ 0 };
    ParallelForOptions options;
    options.GrainSize = 1;
    options.MaxConcurrency = 2;
    Ref<Task> task = system.ParallelFor(
      "Bounded", 8,
      [&](uint32_t) {
          const uint32_t current = active.fetch_add(1, std::memory_order_acq_rel) + 1;
          uint32_t maximum = maximumActive.load(std::memory_order_acquire);
          while (current > maximum && !maximumActive.compare_exchange_weak(maximum, current, std::memory_order_acq_rel))
          {
          }
          gate.EnterAndWait();
          active.fetch_sub(1, std::memory_order_acq_rel);
      },
      options);

    REQUIRE(gate.WaitForEntered(2));
    CHECK(maximumActive.load(std::memory_order_acquire) == 2);
    gate.Open();
    task->Wait();
}

TEST_CASE("ParallelFor stops assigning work after the first failure", "[Threading][TaskSystem]")
{
    TaskSystem system(4);
    std::atomic<uint32_t> calls{ 0 };
    ParallelForOptions options;
    options.GrainSize = 1;
    options.MaxConcurrency = 4;
    Ref<Task> task = system.ParallelFor(
      "Fail fast", 100000,
      [&](uint32_t index) {
          calls.fetch_add(1, std::memory_order_acq_rel);
          if (index == 0)
              throw std::runtime_error("parallel failure");
      },
      options);

    CHECK_THROWS_WITH(task->Wait(), "parallel failure");
    CHECK(task->GetStatus() == TaskStatus::Failed);
    CHECK(calls.load(std::memory_order_acquire) < 100000);
}

TEST_CASE("TaskSystem destruction drains dependency continuations", "[Threading][TaskSystem]")
{
    std::atomic<bool> continuationRan{ false };
    TestGate gate;
    {
        TaskSystem system(1);
        Ref<Task> blocker = system.Submit("Destructor blocker", [&]() { gate.EnterAndWait(); });
        REQUIRE(gate.WaitForEntered(1));
        Ref<Task> prerequisite = system.Submit("Destructor prerequisite", []() {});
        TaskOptions options;
        options.Dependency = prerequisite;
        system.Submit("Destructor continuation", [&]() { continuationRan.store(true, std::memory_order_release); }, options);
        gate.Open();
    }
    CHECK(continuationRan.load(std::memory_order_acquire));
}
