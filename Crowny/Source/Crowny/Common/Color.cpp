#include "cwpch.h"

#include "Crowny/Common/Color.h"

namespace Crowny
{
    Color Color::FromRGBA(const glm::vec4& color)
    {
        const int r = int(color.r * 255.0f);
        const int g = int(color.g * 255.0f);
        const int b = int(color.b * 255.0f);
        const int a = int(color.a * 255.0f);

        return Color(a << 24 | r << 16 | g << 8 | b);
    }

    Color Color::FromRGB(const glm::vec3& color)
    {
        const int r = int(color.r * 255.0f);
        const int g = int(color.g * 255.0f);
        const int b = int(color.b * 255.0f);
        const int a = 255;

        return Color(a << 24 | r << 16 | g << 8 | b);
    }

    Color Color::FromHex(uint32_t value) { return Color(value); }

    Color::Color(uint32_t color) : m_Color(color) {}

    Color Color::Blue = Color(0xff0000ff);
    Color Color::Brown = Color(0xffa52a2a);
    Color Color::Cyan = Color(0xff00ffff);
    Color Color::Gray = Color(0xff808080);
    Color Color::DarkGray = Color(0xffa9a9a9);
    Color Color::LightGray = Color(0xffd3d3d3);
    Color Color::Green = Color(0xff008000);
    Color Color::Yellow = Color(0xffffff00);
    Color Color::Magenta = Color(0xffff00ff);
    Color Color::Purple = Color(0xff800080);
    Color Color::Orange = Color(0xffffa500);
    Color Color::Red = Color(0xffff0000);
    Color Color::Transparent = Color(0x00ffffff);
    Color Color::White = Color(0xffffffff);
    Color Color::Black = Color(0xff000000);

} // namespace Crowny
