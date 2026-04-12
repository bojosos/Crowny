#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <atomic>
#include <functional>

namespace Crowny
{

    class TaskSystem;

    enum class TaskPriority
    {
        Low = 0,
        Normal = 1,
        High = 2
    };

    class Task
    {
    public:
        static Ref<Task> Create(const String& name, std::function<void()> worker, TaskPriority priority = TaskPriority::Normal,
                                Ref<Task> dependency = nullptr);

        bool IsComplete() const { return m_State.load(std::memory_order_acquire) == 2; }
        bool IsCanceled() const { return m_State.load(std::memory_order_acquire) == 3; }
        bool HasStarted() const { return m_State.load(std::memory_order_acquire) >= 1; }

        void Wait();
        void Cancel();

    private:
        friend class TaskSystem;

        Task(const String& name, std::function<void()> worker, TaskPriority priority, Ref<Task> dependency);

        String m_Name;
        TaskPriority m_Priority;
        std::function<void()> m_Worker;
        Ref<Task> m_Dependency;
        std::atomic<uint32_t> m_State{ 0 }; // 0=pending, 1=running, 2=done, 3=canceled

        Mutex m_WaitMutex;
        Signal m_WaitCondition;
    };

    class TaskGroup
    {
    public:
        static Ref<TaskGroup> Create(const String& name, std::function<void(uint32_t)> worker, uint32_t count,
                                     TaskPriority priority = TaskPriority::Normal, Ref<Task> dependency = nullptr);

        bool IsComplete() const { return m_RemainingTasks.load(std::memory_order_acquire) == 0; }

        void Wait();

    private:
        friend class TaskSystem;

        TaskGroup(const String& name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority, Ref<Task> dependency);

        String m_Name;
        uint32_t m_Count;
        TaskPriority m_Priority;
        std::function<void(uint32_t)> m_Worker;
        Ref<Task> m_Dependency;
        std::atomic<uint32_t> m_RemainingTasks;

        Mutex m_WaitMutex;
        Signal m_WaitCondition;
    };

    class TaskSystem
    {
    public:
        TaskSystem();
        ~TaskSystem();

        void Submit(const Ref<Task>& task);
        void Submit(const Ref<TaskGroup>& taskGroup);

        uint32_t GetWorkerCount() const { return (uint32_t)m_Workers.size(); }

        static TaskSystem& Get();

    private:
        struct QueueEntry
        {
            std::function<void()> Work;
            TaskPriority Priority;
        };

        void WorkerLoop(uint32_t workerIndex);
        void Enqueue(std::function<void()> work, TaskPriority priority);
        static void ExecuteTask(const Ref<Task>& task);

        Vector<Thread> m_Workers;
        Vector<QueueEntry> m_TaskQueue;
        Mutex m_QueueMutex;
        Signal m_WorkAvailable;
        bool m_Shutdown = false;
    };

} // namespace Crowny
