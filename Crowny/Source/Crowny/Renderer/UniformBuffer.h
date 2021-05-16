#pragma once

namespace Crowny
{
    
    class UniformBuffer
    {
    public:
        virtual ~UniformBuffer() = default;
            
    public:
        static Ref<UniformBuffer> Create(uint32_t size, BufferUsage usage);
    };
    
}