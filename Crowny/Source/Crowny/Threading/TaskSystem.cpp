#include "cwpch.h"

#include "Crowny/Threading/TaskSystem.h"

#include <tracy/Tracy.hpp>

#include <algorithm>

namespace Crowny
{

    // --- Task ---

    Task::Task(const String& name, std::function<void()> worker, TaskPriority priority, Ref<Task> dependency)
      : m_Name(name), m_Worker(std::move(worker)), m_Priority(priority), m_Dependency(std::move(dependency))
    {
    }

    Ref<Task> Task::Create(const String& name, std::function<void()> worker, TaskPriority priority, Ref<Task> dependency)
    {
        return Ref<Task>(new Task(name, std::move(worker), priority, std::move(dependency)));
    }

    void Task::Wait()
    {
        Lock lock(m_WaitMutex);
        while (m_State.load(std::memory_order_acquire) < 2)
        {
            m_WaitCondition.wait(lock);
        }
    }

    void Task::Cancel()
    {
        uint32_t expected = 0;
        if (m_State.compare_exchange_strong(expected, 3, std::memory_order_acq_rel))
        {
            Lock lock(m_WaitMutex);
            m_WaitCondition.notify_all();
        }
    }

    // --- TaskGroup ---

    TaskGroup::TaskGroup(const String& name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority, Ref<Task> dependency)
      : m_Name(name), m_Worker(std::move(worker)), m_Count(count), m_Priority(priority), m_Dependency(std::move(dependency)), m_RemainingTasks(count)
    {
    }

    Ref<TaskGroup> TaskGroup::Create(const String& name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority,
                                     Ref<Task> dependency)
    {
        return Ref<TaskGroup>(new TaskGroup(name, std::move(worker), count, priority, std::move(dependency)));
    }

    void TaskGroup::Wait()
    {
        Lock lock(m_WaitMutex);
        while (m_RemainingTasks.load(std::memory_order_acquire) > 0)
        {
            m_WaitCondition.wait(lock);
        }
    }

    // --- TaskSystem ---

    TaskSystem::TaskSystem()
    {
        const uint32_t numCores = std::thread::hardware_concurrency();
        // Reserve 2 cores: 1 for sim thread, 1 for render thread
        const uint32_t workerCount = numCores > 3 ? numCores - 2 : 1;

        m_TaskQueue.reserve(256);
        m_Workers.reserve(workerCount);
        for (uint32_t i = 0; i < workerCount; i++)
        {
            m_Workers.emplace_back([this, i]() { WorkerLoop(i); });
        }
    }

    TaskSystem::~TaskSystem()
    {
        {
            Lock lock(m_QueueMutex);
            m_Shutdown = true;
        }
        m_WorkAvailable.notify_all();

        for (auto& worker : m_Workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    void TaskSystem::Enqueue(std::function<void()> work, TaskPriority priority)
    {
        {
            Lock lock(m_QueueMutex);
            m_TaskQueue.push_back({ std::move(work), priority });
        }
        m_WorkAvailable.notify_one();
    }

    void TaskSystem::ExecuteTask(const Ref<Task>& task)
    {
        ZoneScopedN("ExecuteTask");
        ZoneText(task->m_Name.c_str(), task->m_Name.size());

        // Block-wait on dependency if needed. This yields the worker thread
        // to the OS scheduler rather than busy-spinning via re-enqueue.
        if (task->m_Dependency)
            task->m_Dependency->Wait();

        uint32_t expected = 0;
        if (!task->m_State.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
            return;

        task->m_Worker();

        task->m_State.store(2, std::memory_order_release);
        Lock lock(task->m_WaitMutex);
        task->m_WaitCondition.notify_all();
    }

    void TaskSystem::Submit(const Ref<Task>& task)
    {
        Enqueue([captured = task]() { ExecuteTask(captured); }, task->m_Priority);
    }

    void TaskSystem::Submit(const Ref<TaskGroup>& taskGroup)
    {
        const Ref<TaskGroup> captured = taskGroup;
        for (uint32_t i = 0; i < taskGroup->m_Count; i++)
        {
            const uint32_t index = i;

            Enqueue(
              [captured, index]() {
                  // Block-wait on dependency if needed
                  if (captured->m_Dependency)
                      captured->m_Dependency->Wait();

                  captured->m_Worker(index);

                  if (captured->m_RemainingTasks.fetch_sub(1, std::memory_order_acq_rel) == 1)
                  {
                      Lock lock(captured->m_WaitMutex);
                      captured->m_WaitCondition.notify_all();
                  }
              },
              captured->m_Priority);
        }
    }

    void TaskSystem::WorkerLoop(uint32_t workerIndex)
    {
        char threadName[32];
        snprintf(threadName, sizeof(threadName), "Worker %u", workerIndex);
        tracy::SetThreadName(threadName);

        while (true)
        {
            std::function<void()> work;

            {
                Lock lock(m_QueueMutex);
                m_WorkAvailable.wait(lock, [this]() { return m_Shutdown || !m_TaskQueue.empty(); });

                if (m_Shutdown && m_TaskQueue.empty())
                    return;

                // Find highest priority task
                auto bestIt = m_TaskQueue.begin();
                for (auto it = m_TaskQueue.begin(); it != m_TaskQueue.end(); ++it)
                {
                    if (it->Priority > bestIt->Priority)
                        bestIt = it;
                }

                work = std::move(bestIt->Work);
                m_TaskQueue.erase(bestIt);
            }

            {
                ZoneScopedN("WorkerTask");
                work();
            }
        }
    }

} // namespace Crowny
