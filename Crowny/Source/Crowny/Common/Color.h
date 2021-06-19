#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
	class Color
	{
	public:
		/* Creates a color from a vec3(r, g, b) where (r, g, b, a) are between 0-255 */
		static Color FromRGBA(const glm::vec4& color);
		/* Creates a color from a vec3(r, g, b) where (r, g, b) are between 0-255 and the alpha value is 255 */
		static Color FromRGB(const glm::vec3& color);
		/* Creates a color from a hexadecimal value */
		static Color FromHex(uint32_t value);
		/* Returns a uint32_t representation of a 32bit color */
		operator uint32_t () const { return m_Color; }
		/* Returns a RGBA representation of a 32bit color as glm::vec4 */
		operator glm::vec4() const {
			byte a = (m_Color & 0xff000000) >> 24;
			byte r = (m_Color & 0x00ff0000) >> 16;
			byte g = (m_Color & 0x0000ff00) >> 8;
			byte b = (m_Color & 0x000000ff);
			return glm::vec4(r, g, b, a) / 255.0f;
		}

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