#include "cwpch.h"

#include "Platform/Vulkan/VulkanTexture.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Common/VirtualFileSystem.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Crowny
{
	GLenum VulkanTexture2D::TextureChannelToVulkanChannel(TextureChannel channel)
	{
		switch (channel)
		{
			case Crowny::TextureChannel::CHANNEL_RED:                return GL_RED;
			case Crowny::TextureChannel::CHANNEL_RG:                 return GL_RG;
			case Crowny::TextureChannel::CHANNEL_RGB:                return GL_RGB;
			case Crowny::TextureChannel::CHANNEL_RGBA:               return GL_RGBA;
			case Crowny::TextureChannel::CHANNEL_DEPTH_COMPONENT:    return GL_DEPTH_COMPONENT;
			case Crowny::TextureChannel::CHANNEL_BGR:                return GL_BGR;
			case Crowny::TextureChannel::CHANNEL_BGRA:               return GL_BGRA;
			case Crowny::TextureChannel::CHANNEL_STENCIL_INDEX:      return GL_STENCIL_INDEX;
			default: 												 CW_ENGINE_ASSERT(false, "Unknown TextureChannel!"); return GL_NONE;
		}

		return GL_NONE;
	}

	VkFormat VulkanTexture2D::TextureFormatToVulkanFormat(TextureFormat format)
	{
		switch (format)
		{
			case Crowny::TextureFormat::R8:					return VK_FORMAT_R8_UINT;
			case Crowny::TextureFormat::R32I:     			return VK_FORMAT_R32_SINT;
			case Crowny::TextureFormat::RG32F:				return VK_FORMAT_R32G32_SFLOAT;
			case Crowny::TextureFormat::RGB8:     			return VK_FORMAT_R8G8B8_SRGB;
			case Crowny::TextureFormat::RGBA8:				return VK_FORMAT_R8G8B8A8_SRGB;
			case Crowny::TextureFormat::RGBA16F:  			return VK_FORMAT_R16G16B16A16_SFLOAT;
			case Crowny::TextureFormat::RGBA32F:  			return VK_FORMAT_R32G32B32A32_SFLOAT;
			case Crowny::TextureFormat::DEPTH32F:     		return VK_FORMAT_D32_SFLOAT;
			case Crowny::TextureFormat::DEPTH24STENCIL8:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
			default:										CW_ENGINE_ASSERT(false, "Unknown TextureFormat!"); return VK_FORMAT_UNDEFINED;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkSamplerMipmapMode VulkanTexture2D::TextureFilterToVulkanFilter(TextureFilter filter)
	{
		switch (filter)
		{
			case Crowny::TextureFilter::LINEAR:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			case Crowny::TextureFilter::NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			default: 							 CW_ENGINE_ASSERT(false, "Unknown TextureFilter!"); return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}

	VkSamplerAddressMode VulkanTexture2D::TextureWrapToVulkanWrap(TextureWrap wrap)
	{
		switch (wrap)
		{
			case Crowny::TextureWrap::REPEAT:             return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case Crowny::TextureWrap::MIRRORED_REPEAT:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case Crowny::TextureWrap::CLAMP_TO_EDGE:      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case Crowny::TextureWrap::CLAMP_TO_BORDER:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			default: 									  CW_ENGINE_ASSERT(false, "Unknown TextureWrap!"); return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		}

		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}

	GLenum VulkanTexture2D::TextureSwizzleToOpenGLSwizzle(SwizzleType swizzle)
	{
		switch (swizzle)
		{
			case Crowny::SwizzleType::SWIZZLE_RGBA:  return GL_TEXTURE_SWIZZLE_RGBA;
			case Crowny::SwizzleType::SWIZZLE_R:     return GL_TEXTURE_SWIZZLE_R;
			case Crowny::SwizzleType::SWIZZLE_G:     return GL_TEXTURE_SWIZZLE_G;
			case Crowny::SwizzleType::SWIZZLE_B:     return GL_TEXTURE_SWIZZLE_B;
			case Crowny::SwizzleType::SWIZZLE_A:     return GL_TEXTURE_SWIZZLE_A;
			default: 								 CW_ENGINE_ASSERT(false, "Unknown TextureSwizzle!"); return GL_NONE;
		}

		return GL_NONE;
	}

	std::array<VkComponentSwizzle, 4> VulkanTexture2D::TextureSwizzleColorToOpenGLSwizzleColor(SwizzleChannel color)
	{
		std::array<VkComponentSwizzle, 4> res;
		if (swizzle.Type != swizzle.None)
		for (uint32_t i = 0; i < 4; i++)
		{

		}
		return res;
	}

	VkImageLayout VulkanTexture2D::GetOptimalLayout(TextureUsage usage)
	{
		switch (usage)
		{
			case TEXTURE_LOADSTORE:       return VK_IMAGE_LAYOUT_GENERAL;
			case TEXTURE_RENDERTARGET:    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case TEXTURE_DEPTHSTENCIL:    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case TEXTURE_SHADERREAD:
			default:                      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	}

	VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters) : m_Width(width), m_Height(height), m_Parameters(parameters)
	{
		
	}

	VulkanTexture2D::VulkanTexture2D(const std::string& filepath, const TextureParameters& parameters, const std::string& name) 
									: m_FilePath(filepath), m_Parameters(parameters), m_Name(name)
	{
		m_Device = static_cast<VulkanRendererAPI>(RendererAPI::Get()).GetDevices();
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);

		auto [loaded, size] = VirtualFileSystem::Get()->ReadFile(filepath);
		auto* data = stbi_load_from_memory(loaded, size, &width, &height, &channels, 0);

		CW_ENGINE_ASSERT(data, "Failed to load texture!");
		
		m_Width = width;
		m_Height = height;

		if (channels == 4)
		{
			m_Parameters.Format = TextureFormat::RGBA8;
		}
		else if (channels == 3)
		{
			m_Parameters.Format = TextureFormat::RGB8;
		}
		else if (channels == 1)
		{
			m_Parameters.Format = TextureFormat::R8;
		}
		
		VkDevice device = m_Device.GetLogicalDevice();
		
		VkMemoryAllocateInfo memAllocInfo{ };
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		
		VkCommandBuffer copyCmd = m_Device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		
		VkBufferCreateInfo bufferCreateInfo {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_Device.GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		result = vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		result = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		uint8_t* cpyData;
		result = vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void**)&cpyData);
		memcpy(cpyData, data, size);
		vkUnmapMemory(device, stagingMemory);
		
		VkBufferImageCopy bufferCopyRegion{};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = m_Width;
		bufferCopyRegion.imageExtent.height = m_Height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		VkImageCreateInfo imageCreateInfo {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = TextureFormatToVulkanFormat(m_Parameters.Format);
		imageCreateInfo.mipLevels = m_Parameters.MipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VulkanUtils::GetSampleFlags(m_Parameters.Samples);
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { m_Width, m_Height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		
		if (m_Parameters.Usage == TextureUsage::TEXTURE_RENDERTARGET)
			imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else if (m_Parameters.Usage == TextureUsage::TEXTURE_DEPTHSTENCIL)
			imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		
		if (m_Parameters.Usage == TextureUsage::TEXTURE_LOADSTORE)
			imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;


		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &m_Image);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		vkGetImageMemoryRequirements(device, m_Image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_Device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		result = vkAllocateMemory(device, &memAllocInfo, nullptr, &m_DeviceMemory);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		result = vkBindImageMemory(device, m_Image, m_DeviceMemory, 0);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);

		VkImageSubresourceRange subresRange { };
		subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresRange.baseMipLevel = 0;
		subresRange.levelCount = m_Parameters.MipLevels;
		subresRange.layerCount = 1;
		
		VkImageMemoryBarrier imageMemoryBarrier{ };
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.image = m_Image;
		imageMemoryBarrier.subresourceRange = subresRange;
		vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		
		vkCmdCopyBufferToImage(copyCmd, stagingBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

		m_ImageLayout = imageLayout;

		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);

		VkSamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = TextureFilterToVulkanFilter(m_Parameters.Filter);
		samplerCreateInfo.minFilter = TextureFilterToVulkanFilter(m_Parameters.Filter);;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = TextureWrapToVulkanWrap(m_Parameters.Wrap);
		samplerCreateInfo.addressModeV = TextureWrapToVulkanWrap(m_Parameters.Wrap);
		samplerCreateInfo.addressModeW = TextureWrapToVulkanWrap(m_Parameters.Wrap);
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = static_cast<float>(m_Parameters.MipLevels);
		samplerCreateInfo.maxAnisotropy = 1.0f;
		result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_Sampler);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		
		VkImageViewCreateInfo viewCreateInfo{ };
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = nullptr;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 ,1 };
		viewCreateInfo.subresourceRange.levelCount = m_Parameters.MipLevels;
		viewCreateInfo.image = m_Image;
		result = vkCreateImageView(device, &viewCreateInfo, nullptr, &m_ImageView);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		
		if (m_Parameters.GenerateMipmaps)
		{
			m_Parameters.MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = m_Image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;
			
			uint32_t mipWidth = m_Width;
			uint32_t mipHeight = m_Height;
			
			for (uint32_t i = 1; i < m_Parameters.MipLevels; i++)
			{
				barrier.subresourceRange.baseMipLevel = i - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				
				VkImageBlit blit{};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				
				vkCmdBlitImage(copyCmd, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
				
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				if (mipWidth > 1) mipWidth /= 2;
				if (mipHeight > 1) mipHeight /= 2;
			}
			
			barrier.subresourceRange.baseMipLevel = m_Parameters.MipLevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = GetOptimalLayout(m_Parameters.Usage); // get optimal
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			
			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}
		
		CW_ENGINE_INFO("Loaded texture {0}, {1}x{2}x{3}.", m_FilePath, m_Width, m_Height, channels);
		stbi_image_free(data);
		delete loaded;
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		vkDestroyImageView(m_Device.GetLogicalDevice(), m_Sampler, nullptr);
		vkDestroyImage(m_Device.GetLogicalDevice(), m_Image, nullptr);
		if (m_Sampler)
		{
			vkDestroySampler(device.GetLogicalDevice(), m_Sampler, nullptr);
		}
		vkFreeMemory(device.GetLogicalDevice(), m_DeviceMemory, nullptr)
	}

	void VulkanTexture2D::Clear(int32_t clearColor)
	{
		glClearTexImage(m_RendererID, 0, TextureFormatToOpenGLType(m_Parameters.Format), GL_INT, &clearColor);
	}
	
	void VulkanTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bpp = m_Parameters.Format == TextureFormat::RGBA8 ? 4 : 3; // TODO: Fix this!
		//CW_ENGINE_ASSERT(size == m_Width * m_Height * bpp, "Data must be an entire texture!");
#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#else
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#endif
	}

	void VulkanTexture2D::SetData(void* data, TextureChannel channel)
	{
#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, TextureChannelToOpenGLChannel(channel), GL_UNSIGNED_BYTE, data);
#else
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureChannelToOpenGLChannel(channel), GL_UNSIGNED_BYTE, data);
#endif
	}

	void VulkanTexture2D::Bind(uint32_t slot) const
	{
#ifdef MC_WEB
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
#else
		glBindTextureUnit(slot, m_RendererID);
#endif
	}

	void VulkanTexture2D::Unbind(uint32_t slot) const
	{
		glBindTextureUnit(slot, 0);
	}

}