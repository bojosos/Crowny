#pragma once

namespace Crowny
{

    class Bitwise
    {
    public:
        template <typename T> static bool IsPow2(T n) { return (n & (n - 1)) == 0; }

        static uint32_t UnormToUint(float value, uint32_t bits)
        {
            if (value <= 0.0f)
                return 0;
            if (value >= 1.0f)
                return 1;
            return (int)glm::round(value * (1 << bits));
        }

        static uint32_t SnormToUint(float value, uint32_t bits) { return UnormToUint((value + 1.0f) * 0.5f, bits); }

        static float UintToUnorm(uint32_t value, uint32_t bits) { return (float)value / (float(1 << bits) - 1); }

        static float UintToSnorm(uint32_t value, uint32_t bits) { return UintToUnorm(value, 8) * 2.0f - 1.0f; }

        static void IntWrite(void* dst, const int32_t n, const uint32_t value)
        {
            switch (n)
            {
            case 1:
                ((uint8_t*)dst)[0] = (uint8_t)value;
                break;
            case 2:
                ((uint16_t*)dst)[0] = (uint16_t)value;
                break;
            case 3:
                CW_ENGINE_ASSERT(false, "3-byte int write not implemented.");
                break;
            case 4:
                ((uint32_t*)dst)[0] = (uint32_t)value;
                break;
            }
        }

        static uint32_t IntRead(const void* src, int32_t n)
        {
            switch (n)
            {
            case 1:
                return ((uint8_t*)src)[0];
            case 2:
                return ((uint16_t*)src)[0];
            case 3:
                CW_ENGINE_ASSERT(false, "3-byte int read not implemented.");
                break;
            case 4:
                return ((uint32_t*)src)[0];
            default:
                return 0;
            }
        }

        static uint16_t FloatToHalf(float i)
        {
            union {
                float f;
                uint32_t i;
            } v;
            v.f = i;
            return FloatToHalf(v.i);
        }

        static uint16_t FloatToHalf(uint32_t i)
        {
            int32_t s = (i >> 16) & 0x00008000;
            int32_t e = ((i >> 23) & 0x000000ff) - (127 - 15);
            int32_t m = i & 0x007fffff;

            if (e <= 0)
            {
                if (e < -10)
                {
                    return 0;
                }
                m = (m | 0x00800000) >> (1 - e);
                return static_cast<uint16_t>(s | (m >> 13));
            }
            else if (e == 0xff - (127 - 15))
            {
                if (m == 0) // inf
                {
                    return static_cast<uint16_t>(s | 0x7c00);
                }
                else // nan
                {
                    m >>= 13;
                    return static_cast<uint16_t>(s | 0x7c00 | m | (m == 0));
                }
            }
            else
            {
                if (e > 30)
                {
                    return static_cast<uint16_t>(s | 0x7c00);
                }

                return static_cast<uint16_t>(s | (e << 10) | (m >> 13));
            }
        }

        static float HalfToFloat(uint16_t y)
        {
            union {
                float f;
                uint32_t i;
            } v;
            v.i = HalfToFloatInt(y);
            return v.f;
        }

        static uint32_t HalfToFloatInt(uint16_t v)
        {
            int32_t s = (v >> 15) & 0x00000001;
            int32_t e = (v >> 10) & 0x0000001f;
            int32_t m = v & 0x000003ff;

            if (e == 0)
            {
                if (m == 0)
                    return s << 31;
                else
                {
                    while (!(m & 0x00000400))
                    {
                        m <<= 1;
                        e -= 1;
                    }

                    e += 1;
                    m &= ~0x00000400;
                }
            }
            else if (e == 31)
            {
                if (m == 0)
                    return (s << 31) | 0x7f800000;
                else
                    return (s << 31) | 0x7f800000 | (m << 13);
            }

            e = e + (127 - 15);
            m = m << 13;
            return (s << 31) | (e << 23) | m;
        }
    };
} // namespace Crowny
