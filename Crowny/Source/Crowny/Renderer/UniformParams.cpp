#include "cwpch.h"

#include "Crowny/Renderer/UniformParams.h"

namespace Crowny
{

    void UniformParams::SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBlockBuffer>& uniformBlock)
    {
        uint32_t globaSlot = mParamInfo->getSequentialSlot(GpuPipelineParamInfo::ParamType::ParamBlock, set, slot);
        if (globalSlot == -1)
            return;

        m_BufferBlocks[globalSlot] = uniformBlock;
    }

}