#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Common/StdHeaders.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>

namespace Crowny
{
    class Task;
    class TaskSystem;
    class TaskSystemTestAccess;
    struct TaskSchedulerControl;

    enum class TaskPriority
    {
        Low = 0,
        Normal = 1,
        High = 2
    };

    enum class TaskStatus : uint8_t
    {
        Waiting,
        Queued,
        Running,
        Succeeded,
        Canceled,
        Failed
    };

    class TaskContext
    {
    public:
        bool IsCancellationRequested() const;

    private:
        friend class TaskSystem;
        explicit TaskContext(const Task& task) : m_Task(task) {}

        const Task& m_Task;
    };

    class Task : public RefCounted
    {
    public:
        // Compatibility factory. New callers should prefer TaskSystem::Submit.
        static Ref<Task> Create(const String& name, std::function<void()> worker, TaskPriority priority = TaskPriority::Normal,
                                Ref<Task> dependency = nullptr);

        TaskStatus GetStatus() const { return m_Status.load(std::memory_order_acquire); }
        bool IsComplete() const;
        bool IsCanceled() const { return GetStatus() == TaskStatus::Canceled; }
        bool HasStarted() const { return m_HasStarted.load(std::memory_order_acquire); }
        bool IsCancellationRequested() const;
        uint64_t GetGeneration() const { return m_Generation.load(std::memory_order_acquire); }

        void Wait();
        bool WaitFor(std::chrono::milliseconds timeout);
        bool Cancel();

    private:
        friend class TaskContext;
        friend class TaskGroup;
        friend class TaskSystem;

        Task(String name, std::function<void(TaskContext&)> worker, TaskPriority priority, Ref<Task> dependency);

        static bool IsTerminal(TaskStatus status);
        Vector<Ref<Task>> TransitionTerminal(TaskStatus status, std::exception_ptr failure = nullptr);
        static void PropagateContinuations(Vector<Ref<Task>> continuations, TaskStatus status, const std::exception_ptr& failure);
        void PropagateTerminal(TaskStatus status, const std::exception_ptr& failure);
        void ThrowIfFailed() const;

        String m_Name;
        TaskPriority m_Priority;
        std::function<void(TaskContext&)> m_Worker;
        Ref<Task> m_Dependency;
        std::atomic<TaskStatus> m_Status{ TaskStatus::Waiting };
        std::atomic<bool> m_HasStarted{ false };
        std::atomic<bool> m_CancelRequested{ false };
        std::atomic<uint64_t> m_Generation{ 0 };
        std::atomic<Task*> m_ExecutionParent{ nullptr };

        mutable Mutex m_StateMutex;
        Signal m_CompletionCondition;
        std::exception_ptr m_Failure;
        Vector<Ref<Task>> m_Continuations;
        std::shared_ptr<TaskSchedulerControl> m_Control;
    };

    struct TaskOptions
    {
        TaskPriority Priority = TaskPriority::Normal;
        Ref<Task> Dependency;
    };

    struct ParallelForOptions
    {
        TaskPriority Priority = TaskPriority::Normal;
        Ref<Task> Dependency;
        uint32_t GrainSize = 0;
        uint32_t MaxConcurrency = 0;
    };

    class TaskGroup : public RefCounted
    {
    public:
        // Compatibility wrapper around TaskSystem::ParallelFor.
        static Ref<TaskGroup> Create(const String& name, std::function<void(uint32_t)> worker, uint32_t count,
                                     TaskPriority priority = TaskPriority::Normal, Ref<Task> dependency = nullptr);

        TaskStatus GetStatus() const;
        bool IsComplete() const;
        bool IsCanceled() const { return GetStatus() == TaskStatus::Canceled; }
        bool HasStarted() const;
        void Wait();
        bool Cancel();

    private:
        friend class TaskSystem;

        TaskGroup(String name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority, Ref<Task> dependency);

        String m_Name;
        uint32_t m_Count;
        TaskPriority m_Priority;
        std::function<void(uint32_t)> m_Worker;
        Ref<Task> m_Dependency;
        mutable Mutex m_StateMutex;
        Ref<Task> m_Completion;
        bool m_Submitted = false;
        bool m_CancelRequested = false;
    };

    class TaskSystem : public Module<TaskSystem>
    {
    public:
        explicit TaskSystem(uint32_t workerCount = 0);
        ~TaskSystem() override;

        // Compatibility submission for tasks created through Task::Create.
        void Submit(const Ref<Task>& task);
        void Submit(const Ref<TaskGroup>& taskGroup);

        Ref<Task> Submit(const String& name, std::function<void()> worker, const TaskOptions& options = {});
        Ref<Task> Submit(const String& name, std::function<void(TaskContext&)> worker, const TaskOptions& options = {});
        Ref<Task> ParallelFor(const String& name, uint32_t count, std::function<void(uint32_t)> worker, const ParallelForOptions& options = {});

        void Drain();
        void CancelPendingAndDrain();

        uint32_t GetWorkerCount() const { return static_cast<uint32_t>(m_Workers.size()); }
        uint64_t GetGeneration() const { return m_Generation; }
        bool IsAccepting() const;

    private:
        friend class Task;
        friend class TaskSystemTestAccess;

        enum class Lifecycle : uint8_t
        {
            Accepting,
            Draining,
            Canceling,
            Stopped
        };

        void WorkerLoop(uint32_t workerIndex);
        bool TryTakeTask(Ref<Task>& task, bool waitForWork);
        bool TakeReadyTaskLocked(Ref<Task>& task);
        bool HasReadyTaskLocked() const;
        void QueueAcceptedTask(const Ref<Task>& task);
        void QueueContinuation(const Ref<Task>& task);
        void ResolveContinuations(Vector<Ref<Task>> continuations, TaskStatus dependencyStatus, const std::exception_ptr& dependencyFailure);
        void ExecuteTask(const Ref<Task>& task);
        void BeginTaskWait(Task& waiter, Task& target);
        void EndTaskWait(Task& waiter);
        void WaitForTask(Task& task);
        bool WaitForTask(Task& task, std::chrono::steady_clock::time_point deadline);
        void Stop(bool cancelPending);

        static thread_local TaskSystem* s_CurrentWorkerSystem;
        static thread_local Task* s_CurrentTask;

        Vector<Thread> m_Workers;
        Array<Deque<Ref<Task>>, 3> m_ReadyQueues;
        mutable Mutex m_QueueMutex;
        Signal m_WorkAvailable;
        Mutex m_ShutdownMutex;
        Mutex m_WaitGraphMutex;
        UnorderedMap<Task*, Task*> m_WaitEdges;
        Mutex m_TestHookMutex;
        std::atomic<bool> m_HasBeforeSubmitForTests{ false };
        std::atomic<bool> m_HasBeforeParallelRunnerSubmitForTests{ false };
        std::function<void()> m_BeforeSubmitForTests;
        std::function<void()> m_BeforeParallelRunnerSubmitForTests;
        std::shared_ptr<TaskSchedulerControl> m_Control;
        std::atomic<Lifecycle> m_Lifecycle{ Lifecycle::Accepting };
        uint64_t m_Generation = 0;
        size_t m_FairnessCursor = 0;
        bool m_StopWorkers = false;
    };

    class TaskSystemTestAccess
    {
    public:
        static void SetBeforeSubmit(TaskSystem& taskSystem, std::function<void()> hook);
        static void SetBeforeParallelRunnerSubmit(TaskSystem& taskSystem, std::function<void()> hook);
    };
} // namespace Crowny
