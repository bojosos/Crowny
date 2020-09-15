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
		RGBA,
		RED
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

	enum class TextureShape
	{
		TEXTURE_2D, 
		TEXTURE_CUBE
	};

	enum class TextureType
	{
		TEXTURE_SPRITE, 
		TEXTURE_DEFAULT, 
		TEXTURE_CURSOR, 
		TEXTURE_NORMAL, 
		TEXTURE_LIGHTMAP, 
		TEXTURE_SINGLECHANNEL
	};

	struct TextureParameters
	{
		TextureParameters() = default;

		TextureParameters(TextureFormat format, TextureFilter filter, TextureWrap wrap, TextureSwizzle swizzle = {}) : Format(format), Filter(filter), Wrap(wrap), Swizzle(swizzle)
		{

		}

		TextureType Type = TextureType::TEXTURE_DEFAULT;
		TextureShape Shape = TextureShape::TEXTURE_2D;

		bool sRGB = true;
		bool ReadWrite = false;
		bool GenerateMipmaps = false;

		TextureFormat Format = TextureFormat::RGBA;
		TextureFilter Filter = TextureFilter::NEAREST;
		TextureWrap Wrap = TextureWrap::REPEAT;
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
		virtual void Unbind(uint32_t slot) const = 0;
		virtual bool operator==(const Texture& other) const = 0;

		virtual const std::string& GetName() const = 0;

		static Ref<Texture> Create(const TextureParameters& parameters);
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, const TextureParameters& parameters = {});
		static Ref<Texture2D> Create(const std::string& filepath, const TextureParameters& parameters = {});
	};

	class TextureCube : public Texture
	{
	protected:
		enum class InputFormat
		{
			HORIZONTAL_CROSS,
			VERTICAL_CROSS
		};

	public:
		//Creates a texture from a file
		static Ref<TextureCube> Create(const std::string& filepath, const TextureParameters& parameters = {});
		//Creates a texture from files
		static Ref<TextureCube> Create(const std::array<std::string, 6>& files, const TextureParameters& parameters = {});
		//Creates a texture from V Cross
		static Ref<TextureCube> Create(const std::array<std::string, 6>& files, uint32_t mips, const TextureParameters& parameters = {});
		
	};
}