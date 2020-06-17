#pragma once

namespace Crowny
{

	enum class TextureChannel
	{
		NONE,
		CHANNEL_RED,
		CHANNEL_RG,
		CHANNEL_RGB,
		CHANNEL_BGR,
		CHANNEL_RGBA,
		CHANNEL_BGRA,
		CHANNEL_DEPTH_COMPONENT,
		CHANNEL_STENCIL_INDEX
	};

	enum class TextureWrap
	{
		NONE = 0,
		REPEAT,
		MIRRORED_REPEAT,
		CLAMP_TO_EDGE,
		CLAMP_TO_BORDER
	};

	enum class TextureFilter
	{
		NONE = 0,
		LINEAR,
		NEAREST
	};

	enum class TextureFormat
	{
		NONE = 0,
		RGB,
		RGBA
	};

	enum class SwizzleType
	{
		NONE = 0,
		SWIZZLE_RGBA,
		SWIZZLE_R,
		SWIZZLE_G,
		SWIZZLE_B,
		SWIZZLE_A
	};

	enum class SwizzleChannel
	{
		NONE = 0,
		RED,
		GREEN,
		BLUE,
		ALPHA,
		ONE,
		ZERO
	};

	struct TextureSwizzle
	{
		SwizzleType Type;
		SwizzleChannel Swizzle[4];

		TextureSwizzle()
		{
			Type = SwizzleType::NONE;
			Swizzle[0] = SwizzleChannel::NONE;
		}

		TextureSwizzle(SwizzleType type, SwizzleChannel swizzle) : Type(type)
		{
			CW_ENGINE_ASSERT(type != SwizzleType::SWIZZLE_RGBA, "A SWIZZLE_RGBA requires 4 parameters!");
			Swizzle[0] = swizzle;
		}

		TextureSwizzle(SwizzleType type, SwizzleChannel ar[4]) : Type(type)
		{
			CW_ENGINE_ASSERT(type == SwizzleType::SWIZZLE_RGBA, "A four parameter TEXTURE_SWIZZLE has to be RGBA!");
			Swizzle[0] = ar[0];
			Swizzle[1] = ar[1];
			Swizzle[2] = ar[2];
			Swizzle[3] = ar[3];
		}

	};


	struct TextureParameters
	{
		TextureParameters()
		{
			Format = TextureFormat::RGBA;
			Filter = TextureFilter::NEAREST;
			Wrap = TextureWrap::REPEAT;
		}

		TextureParameters(TextureFormat format, TextureFilter filter, TextureWrap wrap, TextureSwizzle swizzle = {}) : Format(format), Filter(filter), Wrap(wrap), Swizzle(swizzle)
		{

		}

		TextureFormat Format;
		TextureFilter Filter;
		TextureWrap Wrap;
		TextureSwizzle Swizzle;
	};

	class Texture
	{
	public:
		virtual ~Texture() = default;
		
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;
		virtual void SetData(void* data, TextureChannel channel) = 0;

		virtual void Bind(uint32_t slot) const = 0;
		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, const TextureParameters& parameters = {});
		static Ref<Texture2D> Create(const std::string& filepath, const TextureParameters& parameters = {});
	};
}