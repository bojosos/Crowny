#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
/*
	struct VulkanImageDesc
	{
		VkImage Image;
		VkDeviceMemory Memory;
		VkImageLayout Layout;
		VkFormat Foramt;
		uint32_t Faces;
		uint32_t NumMips;
		uint32_t Usage;
		TextureShape Shape;
	};

	class VulkanImageSubresource
	{
	public:
		VulkanImageSubresource(VkImageLayout layout);
		VkImageLayout GetLayout() const { return m_Layout; }
		void SetLayout(VkImageLayout layout)  { m_Layout = layout; }
	private:
		VkImageLayout m_Layout;
	};

	class VulkanImage
	{
	public:
		VulkanImage(VkImage image, VkDeviceMemory memory, VkImageLayout layout, VkFormat format, const TextureParameters& props, bool ownsImage = true);
		VulkanImage(VulkanImageDesc& desc, bool ownsImage = true);
		~VulkanImage();

		VkImage GetHandle() const { return m_Image; }
		VkImageView GetView(bool framebuffer) const;
		VkImageView GetView(VkFormat format, bool framebuffer) const;
		VkImageAspectFlags GetAspectFlags() const;
		VkImageSubresourceRange GetRange() const;
		VulkanImageSubresource GetSubresource(uint32_t face, uint32_t mipLevel);
	//	void Map(uint32_t face, uint32_t mipLevel, ImageData& output) const;
//		void Unmap();
		VkAccessFlags GetAccessFlags(VkImageLayout layout, bool readonly = false);
		
	private:
		VkImageView CreateView(VkFormat format , VkImageAspectFlags mask) const;

		struct ImageViewInfo
		{
			bool Framebuffer;
			VkImageView View;
			VkFormat Format;
		};

		VkImage m_Image;
		VkDeviceMemory m_Allocation;
		VkImageView m_MainView;
		VkImageView m_FramebufferMainView;
		int32_t m_Usage;
		bool m_OwnsImage;
		uint32_t m_NumFaces;
		uint32_t m_NumMipLevels;
		VulkanImageSubresource** m_Subresources;

		mutable VkImageViewCreateInfo m_ImageViewCI;
		mutable std::vector<ImageViewInfo> m_ImageInfos;
	};

	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters);
		VulkanTexture2D(const std::string& filepath, const TextureParameters& parameters, const std::string& name);
		~VulkanTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual const std::string& GetName() const override { return ""; }
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

*/
}