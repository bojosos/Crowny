#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{

	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters);
		VulkanTexture2D(const std::string& filepath, const TextureParameters& parameters, const std::string& name);
		~VulkanTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual const std::string& GetName() const override { return m_Name; }
		virtual const std::string& GetFilepath() const override { return m_FilePath; }

		virtual uint32_t GetRendererID() const override { return m_RendererID; };

		virtual void Bind(uint32_t slot) const override;
		virtual void Unbind(uint32_t slot) const override;
		virtual void Clear(int32_t clearColor) override;
		virtual void SetData(void* data, uint32_t size) override;
		virtual void SetData(void* data, TextureChannel channel = TextureChannel::CHANNEL_RGBA) override;

		virtual bool operator==(const Texture& other) const override
		{
			return (other.GetRendererID() == m_RendererID);
		}

	private:
		TextureParameters m_Parameters;
		uint32_t m_RendererID;
		std::string m_FilePath;
		uint32_t m_Width, m_Height;
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageLayout m_ImageLayout;
		VkImageView m_ImageView;
		VkDeviceMemory m_DeviceMemory;
		VkDescriptorImageInfo m_Descriptor;
		VkSampler m_Sampler;
		
	private:
		static VkImageLayout GetOptimalLayout();
		static uint32_t TextureChannelToVulkanChannel(TextureChannel channel);
		static VkFormat TextureFormatToVulkanFormat(TextureFormat format);
		static VkSampleCountFlagBits TextureSamplesToVulkanSamples(uint32_t samples);
		static VkSamplerMipmapMode TextureFilterToVulkanFilter(TextureFilter filter);
		static VkSamplerAddressMode TextureWrapToVulkanWrap(TextureWrap wrap);
		static std::array<VkComponentSwizzle, 4> TextureSwizzleToVulkanSwizzle(SwizzleType swizzle);
	};


}