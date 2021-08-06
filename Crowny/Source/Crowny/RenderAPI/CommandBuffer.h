#pragma once

namespace Crowny
{

    enum class CommandBufferState
    {
        Empty,
        Recording,
        Executing,
        Done
    };

    class CommandBuffer
    {
    public:
        CommandBuffer(GpuQueueType queueType, uint32_t queueIdx, bool secondary);
        virtual ~CommandBuffer() = default;

        GpuQueueType GetType() const { return m_Type; }
        uint32_t GetQueueIdx() const { return m_QueueIdx; }
        virtual CommandBufferState GetState() const = 0;
        virtual void Reset() = 0;

    public:
        static Ref<CommandBuffer> Create(GpuQueueType type, uint32_t queueIdx = 0, bool secondary = false);

    protected:
        bool m_IsSecondary;
        GpuQueueType m_Type;
        uint32_t m_QueueIdx;
    };

    class CommandSyncMask
    {
    public:
        void AddDependency(const Ref<CommandBuffer>& buffer);
        uint32_t GetMask() const { return m_Mask; }
        static uint32_t GetGlobalQueueMask(GpuQueueType type, uint32_t queueIdx);
        static uint32_t GetGlobalQueueIdx(GpuQueueType type, uint32_t queueIdx);
        static uint32_t GetQueueIdxAndType(uint32_t globalQueueIdx, GpuQueueType& type);

    private:
        uint32_t m_Mask = 0;
    };

} // namespace Crowny