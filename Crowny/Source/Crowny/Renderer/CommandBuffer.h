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
        virtual ~CommandBuffer() = default;
        
        GpuQueueType GetType() const { return m_Type; }
        uint32_t GetQueueIdx() const { return m_QueueIdx; }
        virtual CommandBufferState GetState() const = 0;
        virtual void Reset() = 0;
    public:
        static Ref<CommandBuffer> Create(GpuQueueType type);
        
    protected:
        GpuQueueType m_Type;
        uint32_t m_QueueIdx;        
    };

}