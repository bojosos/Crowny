#include "cwpch.h"

#include "Crowny/Utils/PixelUtils.h"

namespace Crowny
{

    glm::ivec2 PixelUtils::GetBlockDimensions(TextureFormat format)
    {
        return glm::ivec2(1, 1);
    }
    
    uint32_t PixelUtils::GetBlockSize(TextureFormat format)
    {
        return GetNumBytes(format);
    }

    void PixelUtils::GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch, uint32_t& depthPitch)
    {
        uint32_t blockSize = GetBlockSize(format);
        rowPitch = width * blockSize;
        depthPitch = width * height * blockSize;
    }
    
    void PixelUtils::GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, uint32_t& mipWidth, uint32_t& mipHeight, uint32_t& mipDepth)
    {
        mipWidth = width;
        mipHeight = height;
        mipDepth = depth;
        
        for (uint32_t i = 0; i < mipLevel; i++)
        {
            if (mipWidth != 1) mipWidth /= 2;
            if (mipHeight != 1) mipHeight /= 2;
            if (mipDepth != 1) mipDepth /= 2;
        }
    }
    
    uint32_t PixelUtils::GetNumBytes(TextureFormat format)
    {
        switch(format)
        {
            case TextureFormat::R8:              return 1;
            case TextureFormat::RG8:             return 2;
            case TextureFormat::RGB8:            return 3;
            case TextureFormat::RGBA8:           return 4;
            case TextureFormat::R32I:            return 4;
            case TextureFormat::RG32F:           return 8;
            case TextureFormat::RGBA16F:         return 8;
            case TextureFormat::RGBA32F:         return 16;
            case TextureFormat::DEPTH32F:        return 4;
            case TextureFormat::DEPTH24STENCIL8: return 4;
            default:                             return 0;
        }
        
        return 0;
    }


    PixelData::PixelData(uint32_t width, uint32_t height, uint32_t depth, TextureFormat textureFormat) : m_Format(textureFormat), m_Width(width), m_Height(height), m_Depth(depth)
    {
        PixelUtils::GetPitch(width, height, depth, textureFormat, m_RowPitch, m_SlicePitch);
        m_Buffer = new uint8_t[GetSize()];
    }
    
    PixelData::~PixelData()
    {
        delete[] m_Buffer;
    }

    void PixelData::SetBuffer(uint8_t* data)
    {
        m_Buffer = data;
        //if (m_Buffer) TODO: bad 
         //   delete[] m_Buffer;
    }
    
}