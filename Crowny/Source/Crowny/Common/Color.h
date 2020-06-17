#pragma once
#include <glm/glm.hpp>

namespace Crowny
{
	class Color
	{
	public:
		glm::vec4 GetRGBA();
		inline uint32_t GetColor() const { return m_Color; }

		/* Creates a color from a vec3(r, g, b) where (r, g, b, a) are between 0-255 */
		static Color FromRGBA(const glm::vec4& color);
		/* Creates a color from a vec3(r, g, b) where (r, g, b) are between 0-255 and the alpha value is 255 */
		static Color FromRGB(const glm::vec3& color);
		/* Creates a color from a hexadecimal value */
		static Color FromHex(uint32_t value);
		
		operator uint32_t () const { return m_Color; }

		static Color Black;
		static Color Blue;
		static Color Brown;
		static Color Cyan;
		static Color DarkGray;
		static Color Gray;
		static Color Green;
		static Color LightGray;
		static Color Magenta;
		static Color Orange;
		static Color Purple;
		static Color Red;
		static Color Transparent;
		static Color White;
		static Color Yellow;

	private:
		uint32_t m_Color;
		Color(uint32_t color);
	};
}