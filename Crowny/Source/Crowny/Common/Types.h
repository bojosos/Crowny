#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

	using byte = uint8_t;

	struct Rect2F
	{
		float X, Y, Width, Height;

		Rect2F() : X(0), Y(0), Width(0), Height(0) { }
		Rect2F(float x, float y) : X(x), Y(y), Width(0), Height(0) { }
		Rect2F(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) { }

		bool Contains(const glm::vec2& pos)
		{
			return (X <= pos.x && Y <= pos.y && X + Width >= pos.x && Y + Height >= pos.y);
		}

    bool operator==(const Rect2F& other)
    {
      return X == other.X && Y == other.Y && Width == other.Width && Height == other.Height;
    }
	};
	
	struct Rect2I
	{
		int32_t X, Y, Width, Height;

		Rect2I() : X(0), Y(0), Width(0), Height(0) { }
		Rect2I(int32_t x, int32_t y) : X(x), Y(y), Width(0), Height(0) { }
		Rect2I(int32_t x, int32_t y, int32_t width, int32_t height) : X(x), Y(y), Width(width), Height(height) { }
	};

	struct Padding
	{
		float Left, Right, Top, Bottom;

		Padding() : Left(0), Right(0), Top(0), Bottom(0) { }
		Padding(float padding) : Left(padding), Right(padding), Top(padding), Bottom(padding) { }
		Padding(float vertical, float horizontal) : Left(horizontal), Right(horizontal), Top(vertical), Bottom(vertical) { }
		Padding(float top, float right, float bottom, float left) : Top(top), Left(left), Right(right), Bottom(bottom) { }
	};
	
	//TODO: refactor these enums, make same style
	enum ShaderType
	{
		VERTEX_SHADER,
		FRAGMENT_SHADER,
		GEOMETRY_SHADER,
		DOMAIN_SHADER,
		HULL_SHADER,
		COMPUTE_SHADER,
		SHADER_COUNT
	};
	
	enum GpuBufferUsage
	{
		STATIC_DRAW,
		DYNAMIC_DRAW
	};

	enum GpuQueueType
	{
		GRAPHICS_QUEUE,
		COMPUTE_QUEUE,
		UPLOAD_QUEUE,
		QUEUE_COUNT
	};

	enum class IndexType
	{
		Index_16,
		Index_32
	};

	enum class BufferUsage
	{
		STATIC_DRAW,
		DYNAMIC_DRAW
	};
	
	enum class GpuLockOptions
	{
		READ_ONLY,
		WRITE_ONLY,
		WRITE_DISCARD,
		WRITE_DISCARD_RANGE
	};
	
	enum class DrawMode
	{
		POINT_LIST,
		LINE_LIST,
		LINE_STRIP,
		TRIANGLE_LIST,
		TRIANGLE_STRIP,
		TRIANGLE_FAN
	};

}
