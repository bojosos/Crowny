#include "cwpch.h"

#include "Crowny/Threading/TaskSystem.h"

#include "Crowny/Common/Assert.h"

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace Crowny
{
    struct TaskSchedulerControl
    {
        std::atomic<uint64_t> Outstanding{ 0 };
        std::atomic<bool> CancelAll{ false };
        Mutex Mutex;
        Signal Changed;
    };

    namespace
    {
        std::atomic<uint64_t> s_NextTaskSystemGeneration{ 1 };

        constexpr std::array<size_t, 13> FAIR_PRIORITY_ORDER = { static_cast<size_t>(TaskPriority::High),   static_cast<size_t>(TaskPriority::High),
                                                                 static_cast<size_t>(TaskPriority::High),   static_cast<size_t>(TaskPriority::High),
                                                                 static_cast<size_t>(TaskPriority::High),   static_cast<size_t>(TaskPriority::High),
                                                                 static_cast<size_t>(TaskPriority::High),   static_cast<size_t>(TaskPriority::High),
                                                                 static_cast<size_t>(TaskPriority::Normal), static_cast<size_t>(TaskPriority::Normal),
                                                                 static_cast<size_t>(TaskPriority::Normal), static_cast<size_t>(TaskPriority::Normal),
                                                                 static_cast<size_t>(TaskPriority::Low) };

        void NotifyTaskFinished(const std::shared_ptr<TaskSchedulerControl>& control)
        {
            if (control == nullptr)
                return;

            uint64_t previous = control->Outstanding.load(std::memory_order_acquire);
            while (true)
            {
                CW_ASSERT(previous > 0, "TaskSystem outstanding task count underflow");
                if (previous == 0)
                    std::terminate();
                if (control->Outstanding.compare_exchange_weak(previous, previous - 1, std::memory_order_acq_rel, std::memory_order_acquire))
                    break;
            }
            if (previous == 1)
            {
                Lock lock(control->Mutex);
                control->Changed.notify_all();
            }
        }
    } // namespace

    thread_local TaskSystem* TaskSystem::s_CurrentWorkerSystem = nullptr;
    thread_local Task* TaskSystem::s_CurrentTask = nullptr;

    bool TaskContext::IsCancellationRequested() const { return m_Task.IsCancellationRequested(); }

    Task::Task(String name, std::function<void(TaskContext&)> worker, TaskPriority priority, Ref<Task> dependency)
      : m_Name(std::move(name)), m_Priority(priority), m_Worker(std::move(worker)), m_Dependency(std::move(dependency))
    {
    }

    Ref<Task> Task::Create(const String& name, std::function<void()> worker, TaskPriority priority, Ref<Task> dependency)
    {
        if (!worker)
            return Ref<Task>(new Task(name, {}, priority, std::move(dependency)));
        return Ref<Task>(new Task(name, [worker = std::move(worker)](TaskContext&) { worker(); }, priority, std::move(dependency)));
    }

    bool Task::IsTerminal(TaskStatus status)
    {
        return status == TaskStatus::Succeeded || status == TaskStatus::Canceled || status == TaskStatus::Failed;
    }

    bool Task::IsComplete() const { return IsTerminal(GetStatus()); }

    bool Task::IsCancellationRequested() const
    {
        if (m_CancelRequested.load(std::memory_order_acquire))
            return true;
        if (m_Generation.load(std::memory_order_acquire) == 0)
            return false;
        const std::shared_ptr<TaskSchedulerControl> control = m_Control;
        return control != nullptr && control->CancelAll.load(std::memory_order_acquire);
    }

    Vector<Ref<Task>> Task::TransitionTerminal(TaskStatus status, std::exception_ptr failure)
    {
        Vector<Ref<Task>> continuations;
        std::shared_ptr<TaskSchedulerControl> control;
        {
            Lock lock(m_StateMutex);
            if (IsTerminal(m_Status.load(std::memory_order_acquire)))
                return continuations;

            m_Failure = std::move(failure);
            m_Status.store(status, std::memory_order_release);
            continuations.swap(m_Continuations);
            control = m_Control;
        }

        m_CompletionCondition.notify_all();
        NotifyTaskFinished(control);
        return continuations;
    }

    void Task::PropagateContinuations(Vector<Ref<Task>> continuations, TaskStatus status, const std::exception_ptr& failure)
    {
        for (const Ref<Task>& continuation : continuations)
            continuation->PropagateTerminal(status, failure);
    }

    void Task::PropagateTerminal(TaskStatus status, const std::exception_ptr& failure)
    {
        PropagateContinuations(TransitionTerminal(status, failure), status, failure);
    }

    void Task::ThrowIfFailed() const
    {
        if (GetStatus() != TaskStatus::Failed)
            return;

        std::exception_ptr failure;
        {
            Lock lock(m_StateMutex);
            failure = m_Failure;
        }
        if (failure != nullptr)
            std::rethrow_exception(failure);
        throw std::runtime_error("Task failed without an exception");
    }

    void Task::Wait()
    {
        if (!IsComplete())
        {
            if (GetGeneration() == 0)
                throw std::logic_error("Cannot wait for a task that has not been submitted");

            if (TaskSystem::s_CurrentWorkerSystem != nullptr)
            {
                if (TaskSystem::s_CurrentWorkerSystem->GetGeneration() != GetGeneration())
                    throw std::logic_error("A worker cannot wait for a task owned by another scheduler generation");
                TaskSystem::s_CurrentWorkerSystem->WaitForTask(*this);
            }
            else
            {
                Lock lock(m_StateMutex);
                m_CompletionCondition.wait(lock, [this]() { return IsTerminal(m_Status.load(std::memory_order_acquire)); });
            }
        }
        ThrowIfFailed();
    }

    bool Task::WaitFor(std::chrono::milliseconds timeout)
    {
        bool complete = IsComplete();
        if (!complete)
        {
            if (GetGeneration() == 0)
                throw std::logic_error("Cannot wait for a task that has not been submitted");

            const auto deadline = std::chrono::steady_clock::now() + timeout;
            if (TaskSystem::s_CurrentWorkerSystem != nullptr)
            {
                if (TaskSystem::s_CurrentWorkerSystem->GetGeneration() != GetGeneration())
                    throw std::logic_error("A worker cannot wait for a task owned by another scheduler generation");
                complete = TaskSystem::s_CurrentWorkerSystem->WaitForTask(*this, deadline);
            }
            else
            {
                Lock lock(m_StateMutex);
                complete =
                  m_CompletionCondition.wait_until(lock, deadline, [this]() { return IsTerminal(m_Status.load(std::memory_order_acquire)); });
            }
        }
        if (complete)
            ThrowIfFailed();
        return complete;
    }

    bool Task::Cancel()
    {
        Vector<Ref<Task>> continuations;
        std::shared_ptr<TaskSchedulerControl> control;
        bool wasRequested = false;
        {
            Lock lock(m_StateMutex);
            const TaskStatus status = m_Status.load(std::memory_order_acquire);
            if (IsTerminal(status))
                return false;

            wasRequested = m_CancelRequested.exchange(true, std::memory_order_acq_rel);
            if (status == TaskStatus::Running)
                return !wasRequested;

            m_Status.store(TaskStatus::Canceled, std::memory_order_release);
            continuations.swap(m_Continuations);
            control = m_Control;
        }

        m_CompletionCondition.notify_all();
        NotifyTaskFinished(control);
        PropagateContinuations(std::move(continuations), TaskStatus::Canceled, nullptr);
        return !wasRequested;
    }

    TaskGroup::TaskGroup(String name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority, Ref<Task> dependency)
      : m_Name(std::move(name)), m_Count(count), m_Priority(priority), m_Worker(std::move(worker)), m_Dependency(std::move(dependency))
    {
    }

    Ref<TaskGroup> TaskGroup::Create(const String& name, std::function<void(uint32_t)> worker, uint32_t count, TaskPriority priority,
                                     Ref<Task> dependency)
    {
        return Ref<TaskGroup>(new TaskGroup(name, std::move(worker), count, priority, std::move(dependency)));
    }

    TaskStatus TaskGroup::GetStatus() const
    {
        Lock lock(m_StateMutex);
        if (m_Completion)
            return m_Completion->GetStatus();
        if (m_CancelRequested)
            return TaskStatus::Canceled;
        return m_Count == 0 ? TaskStatus::Succeeded : TaskStatus::Waiting;
    }

    bool TaskGroup::IsComplete() const { return Task::IsTerminal(GetStatus()); }

    bool TaskGroup::HasStarted() const
    {
        Lock lock(m_StateMutex);
        return m_Completion && m_Completion->HasStarted();
    }

    void TaskGroup::Wait()
    {
        Ref<Task> completion;
        bool cancelRequested = false;
        {
            Lock lock(m_StateMutex);
            completion = m_Completion;
            cancelRequested = m_CancelRequested;
        }
        if (completion)
            completion->Wait();
        else if (m_Count != 0 && !cancelRequested)
            throw std::logic_error("Cannot wait for a task group that has not been submitted");
    }

    bool TaskGroup::Cancel()
    {
        Lock lock(m_StateMutex);
        if (m_Completion)
        {
            const bool canceled = m_Completion->Cancel();
            if (canceled)
                m_CancelRequested = true;
            return canceled;
        }
        if (m_CancelRequested)
            return false;
        m_CancelRequested = true;
        return true;
    }

    TaskSystem::TaskSystem(uint32_t workerCount)
      : m_Control(std::make_shared<TaskSchedulerControl>()), m_Generation(s_NextTaskSystemGeneration.fetch_add(1, std::memory_order_relaxed))
    {
        if (workerCount == 0)
        {
            const uint32_t numCores = std::thread::hardware_concurrency();
            workerCount = numCores > 3 ? numCores - 2 : 1;
        }
        workerCount = std::max(workerCount, 1u);

        m_Workers.reserve(workerCount);
        try
        {
            for (uint32_t index = 0; index < workerCount; index++)
                m_Workers.emplace_back([this, index]() { WorkerLoop(index); });
        }
        catch (...)
        {
            {
                Lock lock(m_QueueMutex);
                m_StopWorkers = true;
                m_Lifecycle.store(Lifecycle::Stopped, std::memory_order_release);
            }
            m_WorkAvailable.notify_all();
            for (Thread& worker : m_Workers)
                if (worker.joinable())
                    worker.join();
            throw;
        }
    }

    TaskSystem::~TaskSystem()
    {
        try
        {
            Drain();
        }
        catch (...)
        {
            std::terminate();
        }
    }

    bool TaskSystem::IsAccepting() const { return m_Lifecycle.load(std::memory_order_acquire) == Lifecycle::Accepting; }

    void TaskSystem::Submit(const Ref<Task>& task)
    {
        if (!task)
            throw std::invalid_argument("Cannot submit a null task");

        TaskStatus dependencyStatus = TaskStatus::Succeeded;
        std::exception_ptr dependencyFailure;
        bool ready = false;
        bool propagateDependency = false;
        {
            Lock queueLock(m_QueueMutex);
            const Lifecycle lifecycle = m_Lifecycle.load(std::memory_order_acquire);
            const bool internalDrainSubmission = lifecycle == Lifecycle::Draining && s_CurrentWorkerSystem == this;
            if (lifecycle != Lifecycle::Accepting && !internalDrainSubmission)
                throw std::logic_error("TaskSystem is no longer accepting work");

            Lock taskLock(task->m_StateMutex);
            const TaskStatus taskStatus = task->m_Status.load(std::memory_order_acquire);
            if (Task::IsTerminal(taskStatus))
            {
                if (taskStatus == TaskStatus::Canceled && task->m_Generation.load(std::memory_order_acquire) == 0)
                    return;
                throw std::logic_error("Task has already reached a terminal state");
            }
            if (taskStatus != TaskStatus::Waiting || task->m_Generation.load(std::memory_order_acquire) != 0)
                throw std::logic_error("Task has already been submitted");
            if (!task->m_Worker)
                throw std::invalid_argument("Cannot submit a task without work");
            if (task->m_Dependency == task)
                throw std::invalid_argument("A task cannot depend on itself");

            if (task->m_Dependency)
            {
                if (task->m_Dependency->GetGeneration() != m_Generation)
                    throw std::invalid_argument("Task dependency belongs to a different scheduler generation");

                Lock dependencyLock(task->m_Dependency->m_StateMutex);
                dependencyStatus = task->m_Dependency->m_Status.load(std::memory_order_acquire);
                if (!Task::IsTerminal(dependencyStatus))
                    task->m_Dependency->m_Continuations.push_back(task);
                else if (dependencyStatus == TaskStatus::Failed)
                    dependencyFailure = task->m_Dependency->m_Failure;
            }

            if (!task->m_Dependency || dependencyStatus == TaskStatus::Succeeded)
            {
                m_ReadyQueues[static_cast<size_t>(task->m_Priority)].push_back(task);
                ready = true;
            }
            else if (Task::IsTerminal(dependencyStatus))
                propagateDependency = true;

            task->m_Control = m_Control;
            task->m_Generation.store(m_Generation, std::memory_order_release);
            m_Control->Outstanding.fetch_add(1, std::memory_order_acq_rel);
            if (ready)
                task->m_Status.store(TaskStatus::Queued, std::memory_order_release);
        }

        if (ready)
            m_WorkAvailable.notify_one();
        else if (propagateDependency)
            task->PropagateTerminal(dependencyStatus, dependencyFailure);
    }

    void TaskSystem::Submit(const Ref<TaskGroup>& taskGroup)
    {
        if (!taskGroup)
            throw std::invalid_argument("Cannot submit a null task group");

        Lock groupLock(taskGroup->m_StateMutex);
        if (taskGroup->m_Submitted)
            throw std::logic_error("Task group has already been submitted");

        if (taskGroup->m_CancelRequested)
        {
            taskGroup->m_Completion = Task::Create(taskGroup->m_Name, []() {});
            taskGroup->m_Completion->Cancel();
            taskGroup->m_Submitted = true;
            return;
        }

        ParallelForOptions options;
        options.Priority = taskGroup->m_Priority;
        options.Dependency = taskGroup->m_Dependency;
        taskGroup->m_Completion = ParallelFor(taskGroup->m_Name, taskGroup->m_Count, taskGroup->m_Worker, options);
        taskGroup->m_Submitted = true;
    }

    Ref<Task> TaskSystem::Submit(const String& name, std::function<void()> worker, const TaskOptions& options)
    {
        Ref<Task> task = Task::Create(name, std::move(worker), options.Priority, options.Dependency);
        Submit(task);
        return task;
    }

    Ref<Task> TaskSystem::Submit(const String& name, std::function<void(TaskContext&)> worker, const TaskOptions& options)
    {
        Ref<Task> task(new Task(name, std::move(worker), options.Priority, options.Dependency));
        Submit(task);
        return task;
    }

    Ref<Task> TaskSystem::ParallelFor(const String& name, uint32_t count, std::function<void(uint32_t)> worker, const ParallelForOptions& options)
    {
        if (!worker)
            throw std::invalid_argument("ParallelFor requires a worker function");

        const uint32_t workerCount = std::max(GetWorkerCount(), 1u);
        const uint32_t targetChunks = std::max(workerCount * 4u, 1u);
        const uint32_t grainSize = options.GrainSize != 0
                                     ? options.GrainSize
                                     : std::max(1u, static_cast<uint32_t>((static_cast<uint64_t>(count) + targetChunks - 1u) / targetChunks));
        const uint32_t chunkCount = count == 0 ? 1u : static_cast<uint32_t>((static_cast<uint64_t>(count) + grainSize - 1u) / grainSize);
        const uint32_t requestedConcurrency = options.MaxConcurrency != 0 ? options.MaxConcurrency : workerCount;
        const uint32_t runnerCount = std::max(1u, std::min({ requestedConcurrency, workerCount, chunkCount }));

        struct ParallelState
        {
            std::atomic<uint32_t> NextIndex{ 0 };
            std::atomic<bool> Stop{ false };
            uint32_t Count = 0;
            uint32_t GrainSize = 1;
            std::function<void(uint32_t)> Worker;
            Task* Parent = nullptr;
            Mutex FailureMutex;
            std::exception_ptr Failure;

            void RecordFailure(std::exception_ptr failure)
            {
                Lock lock(FailureMutex);
                if (Failure == nullptr)
                    Failure = std::move(failure);
                Stop.store(true, std::memory_order_release);
            }

            std::exception_ptr GetFailure()
            {
                Lock lock(FailureMutex);
                return Failure;
            }
        };

        auto state = std::make_shared<ParallelState>();
        state->Count = count;
        state->GrainSize = grainSize;
        state->Worker = std::move(worker);

        Ref<Task> parent(new Task(name, {}, options.Priority, options.Dependency));
        state->Parent = parent.Get();
        parent->m_Worker = [this, state, runnerCount, name, priority = options.Priority](TaskContext& parentContext) {
            const auto runChunks = [state](TaskContext& context) {
                while (!state->Stop.load(std::memory_order_acquire) && !context.IsCancellationRequested() &&
                       !state->Parent->IsCancellationRequested())
                {
                    const uint32_t begin = state->NextIndex.fetch_add(state->GrainSize, std::memory_order_acq_rel);
                    if (begin >= state->Count)
                        return;
                    const uint32_t end =
                      static_cast<uint32_t>(std::min(static_cast<uint64_t>(begin) + state->GrainSize, static_cast<uint64_t>(state->Count)));
                    for (uint32_t index = begin; index < end; index++)
                    {
                        if (state->Stop.load(std::memory_order_acquire) || context.IsCancellationRequested() ||
                            state->Parent->IsCancellationRequested())
                            return;
                        try
                        {
                            state->Worker(index);
                        }
                        catch (...)
                        {
                            state->RecordFailure(std::current_exception());
                            return;
                        }
                    }
                }
            };

            Vector<Ref<Task>> runners;
            runners.reserve(runnerCount > 0 ? runnerCount - 1u : 0u);
            try
            {
                TaskOptions runnerOptions;
                runnerOptions.Priority = priority;
                for (uint32_t index = 1; index < runnerCount; index++)
                {
                    std::function<void()> submitHook;
                    {
                        Lock lock(m_TestHookMutex);
                        submitHook = m_BeforeParallelRunnerSubmitForTests;
                    }
                    if (submitHook)
                        submitHook();
                    runners.push_back(Submit(name + " runner", runChunks, runnerOptions));
                }
            }
            catch (const std::logic_error&)
            {
                if (!parentContext.IsCancellationRequested())
                    state->RecordFailure(std::current_exception());
                else
                    state->Stop.store(true, std::memory_order_release);
            }
            catch (...)
            {
                state->RecordFailure(std::current_exception());
            }

            runChunks(parentContext);

            for (const Ref<Task>& runner : runners)
            {
                try
                {
                    runner->Wait();
                }
                catch (...)
                {
                    state->RecordFailure(std::current_exception());
                }
            }
            const std::exception_ptr failure = state->GetFailure();
            if (failure != nullptr)
                std::rethrow_exception(failure);
        };

        Submit(parent);
        return parent;
    }

    bool TaskSystem::HasReadyTaskLocked() const
    {
        return std::any_of(m_ReadyQueues.begin(), m_ReadyQueues.end(), [](const Deque<Ref<Task>>& queue) { return !queue.empty(); });
    }

    bool TaskSystem::TakeReadyTaskLocked(Ref<Task>& task)
    {
        for (size_t offset = 0; offset < FAIR_PRIORITY_ORDER.size(); offset++)
        {
            const size_t orderIndex = (m_FairnessCursor + offset) % FAIR_PRIORITY_ORDER.size();
            Deque<Ref<Task>>& queue = m_ReadyQueues[FAIR_PRIORITY_ORDER[orderIndex]];
            if (queue.empty())
                continue;

            task = std::move(queue.front());
            queue.pop_front();
            m_FairnessCursor = (orderIndex + 1) % FAIR_PRIORITY_ORDER.size();
            return true;
        }
        return false;
    }

    bool TaskSystem::TryTakeTask(Ref<Task>& task, bool waitForWork)
    {
        while (true)
        {
            {
                Lock lock(m_QueueMutex);
                if (waitForWork)
                    m_WorkAvailable.wait(lock, [this]() { return m_StopWorkers || HasReadyTaskLocked(); });
                if (m_StopWorkers && !HasReadyTaskLocked())
                    return false;
                if (!TakeReadyTaskLocked(task))
                    return false;
            }

            bool canceled = false;
            {
                Lock lock(task->m_StateMutex);
                if (task->m_Status.load(std::memory_order_acquire) != TaskStatus::Queued)
                {
                    task = nullptr;
                    continue;
                }
                canceled = task->IsCancellationRequested();
                if (!canceled)
                {
                    task->m_HasStarted.store(true, std::memory_order_release);
                    task->m_Status.store(TaskStatus::Running, std::memory_order_release);
                }
            }
            if (canceled)
            {
                task->Cancel();
                task = nullptr;
                continue;
            }
            return true;
        }
    }

    void TaskSystem::QueueAcceptedTask(const Ref<Task>& task)
    {
        {
            Lock queueLock(m_QueueMutex);
            Lock taskLock(task->m_StateMutex);
            if (Task::IsTerminal(task->m_Status.load(std::memory_order_acquire)))
                return;
            if (m_Lifecycle.load(std::memory_order_acquire) == Lifecycle::Canceling ||
                m_Lifecycle.load(std::memory_order_acquire) == Lifecycle::Stopped || task->IsCancellationRequested())
            {
                taskLock.unlock();
                queueLock.unlock();
                task->Cancel();
                return;
            }
            m_ReadyQueues[static_cast<size_t>(task->m_Priority)].push_back(task);
            task->m_Status.store(TaskStatus::Queued, std::memory_order_release);
        }
        m_WorkAvailable.notify_one();
    }

    void TaskSystem::QueueContinuation(const Ref<Task>& task)
    {
        try
        {
            QueueAcceptedTask(task);
        }
        catch (...)
        {
            task->PropagateTerminal(TaskStatus::Failed, std::current_exception());
        }
    }

    void TaskSystem::ResolveContinuations(Vector<Ref<Task>> continuations, TaskStatus dependencyStatus, const std::exception_ptr& dependencyFailure)
    {
        if (dependencyStatus == TaskStatus::Succeeded)
        {
            for (const Ref<Task>& continuation : continuations)
                QueueContinuation(continuation);
            return;
        }

        for (const Ref<Task>& continuation : continuations)
            continuation->PropagateTerminal(dependencyStatus, dependencyFailure);
    }

    void TaskSystem::ExecuteTask(const Ref<Task>& task)
    {
        ZoneScopedN("ExecuteTask");
        ZoneText(task->m_Name.c_str(), task->m_Name.size());

        Task* previousTask = s_CurrentTask;
        task->m_ExecutionParent.store(previousTask, std::memory_order_release);
        s_CurrentTask = task.Get();
        std::exception_ptr failure;
        try
        {
            TaskContext context(*task);
            task->m_Worker(context);
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        s_CurrentTask = previousTask;
        task->m_ExecutionParent.store(nullptr, std::memory_order_release);

        const TaskStatus status = failure != nullptr                ? TaskStatus::Failed
                                  : task->IsCancellationRequested() ? TaskStatus::Canceled
                                                                    : TaskStatus::Succeeded;
        Vector<Ref<Task>> continuations = task->TransitionTerminal(status, failure);
        ResolveContinuations(std::move(continuations), status, failure);
    }

    void TaskSystem::BeginTaskWait(Task& waiter, Task& target)
    {
        for (Task* executionAncestor = &waiter; executionAncestor != nullptr;
             executionAncestor = executionAncestor->m_ExecutionParent.load(std::memory_order_acquire))
        {
            if (executionAncestor == &target)
                throw std::logic_error("Task wait would create a cycle");
        }

        Lock lock(m_WaitGraphMutex);
        if (m_WaitEdges.find(&waiter) != m_WaitEdges.end())
            throw std::logic_error("A task already has an active wait");

        Task* current = &target;
        UnorderedSet<Task*> visited;
        while (current != nullptr && visited.insert(current).second)
        {
            if (current == &waiter)
                throw std::logic_error("Task wait would create a cycle");

            const auto dynamicWait = m_WaitEdges.find(current);
            if (dynamicWait != m_WaitEdges.end())
                current = dynamicWait->second;
            else if (current->GetStatus() == TaskStatus::Waiting)
                current = current->m_Dependency.Get();
            else
                current = nullptr;
        }
        m_WaitEdges.emplace(&waiter, &target);
    }

    void TaskSystem::EndTaskWait(Task& waiter)
    {
        Lock lock(m_WaitGraphMutex);
        const size_t erased = m_WaitEdges.erase(&waiter);
        CW_ASSERT(erased == 1, "TaskSystem wait edge was removed more than once");
    }

    void TaskSystem::WaitForTask(Task& task)
    {
        Task& waiter = *s_CurrentTask;
        BeginTaskWait(waiter, task);

        try
        {
            while (!task.IsComplete())
            {
                Ref<Task> ready;
                if (TryTakeTask(ready, false))
                    ExecuteTask(ready);
                else
                {
                    Lock lock(task.m_StateMutex);
                    task.m_CompletionCondition.wait_for(lock, std::chrono::milliseconds(1),
                                                        [&task]() { return Task::IsTerminal(task.m_Status.load(std::memory_order_acquire)); });
                }
            }
        }
        catch (...)
        {
            EndTaskWait(waiter);
            throw;
        }
        EndTaskWait(waiter);
    }

    bool TaskSystem::WaitForTask(Task& task, std::chrono::steady_clock::time_point deadline)
    {
        Task& waiter = *s_CurrentTask;
        BeginTaskWait(waiter, task);

        bool complete = false;
        try
        {
            Lock lock(task.m_StateMutex);
            complete = task.m_CompletionCondition.wait_until(lock, deadline,
                                                             [&task]() { return Task::IsTerminal(task.m_Status.load(std::memory_order_acquire)); });
        }
        catch (...)
        {
            EndTaskWait(waiter);
            throw;
        }
        EndTaskWait(waiter);
        return complete;
    }

    void TaskSystem::Drain() { Stop(false); }

    void TaskSystem::CancelPendingAndDrain() { Stop(true); }

    void TaskSystem::Stop(bool cancelPending)
    {
        if (s_CurrentWorkerSystem == this)
            throw std::logic_error("TaskSystem shutdown cannot run from one of its workers");

        Lock shutdownLock(m_ShutdownMutex);
        if (m_Lifecycle.load(std::memory_order_acquire) == Lifecycle::Stopped)
            return;

        Vector<Ref<Task>> tasksToCancel;
        {
            Lock queueLock(m_QueueMutex);
            if (cancelPending)
            {
                size_t queuedTaskCount = 0;
                for (const Deque<Ref<Task>>& queue : m_ReadyQueues)
                    queuedTaskCount += queue.size();
                tasksToCancel.reserve(queuedTaskCount);
            }
            m_Lifecycle.store(cancelPending ? Lifecycle::Canceling : Lifecycle::Draining, std::memory_order_release);
            if (cancelPending)
            {
                m_Control->CancelAll.store(true, std::memory_order_release);
                for (Deque<Ref<Task>>& queue : m_ReadyQueues)
                {
                    while (!queue.empty())
                    {
                        tasksToCancel.push_back(std::move(queue.front()));
                        queue.pop_front();
                    }
                }
            }
        }
        m_WorkAvailable.notify_all();

        for (const Ref<Task>& task : tasksToCancel)
            task->Cancel();

        {
            Lock lock(m_Control->Mutex);
            m_Control->Changed.wait(lock, [this]() { return m_Control->Outstanding.load(std::memory_order_acquire) == 0; });
        }

        {
            Lock lock(m_QueueMutex);
            m_StopWorkers = true;
            m_Lifecycle.store(Lifecycle::Stopped, std::memory_order_release);
        }
        m_WorkAvailable.notify_all();
        for (Thread& worker : m_Workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    void TaskSystem::WorkerLoop(uint32_t workerIndex)
    {
        char threadName[32];
        snprintf(threadName, sizeof(threadName), "Worker %u", workerIndex);
        tracy::SetThreadName(threadName);

        s_CurrentWorkerSystem = this;
        while (true)
        {
            Ref<Task> task;
            if (!TryTakeTask(task, true))
                break;
            {
                ZoneScopedN("WorkerTask");
                ExecuteTask(task);
            }
        }
        s_CurrentTask = nullptr;
        s_CurrentWorkerSystem = nullptr;
    }

    void TaskSystemTestAccess::SetBeforeParallelRunnerSubmit(TaskSystem& taskSystem, std::function<void()> hook)
    {
        Lock lock(taskSystem.m_TestHookMutex);
        taskSystem.m_BeforeParallelRunnerSubmitForTests = std::move(hook);
    }
} // namespace Crowny
