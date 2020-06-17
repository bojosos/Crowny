#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

	struct Rectangle
	{
		float X, Y, Width, Height;

		Rectangle() : X(0), Y(0), Width(0), Height(0) { }
		Rectangle(float x, float y) : X(x), Y(y), Width(0), Height(0) { }
		Rectangle(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) { }

		bool Contains(const glm::vec2& pos)
		{
			return (X <= pos.x && Y <= pos.y && X + Width >= pos.x && Y + Height >= pos.y);
		}
	};

	struct Padding
	{
		float Left, Right, Top, Bottom;

		Padding() : Left(0), Right(0), Top(0), Bottom(0) { }
		Padding(float padding) : Left(padding), Right(padding), Top(padding), Bottom(padding) { }
		Padding(float vertical, float horizontal) : Left(horizontal), Right(horizontal), Top(vertical), Bottom(vertical) { }
		Padding(float top, float right, float bottom, float left) : Top(top), Left(left), Right(right), Bottom(Bottom) { }
	};

}