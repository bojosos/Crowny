#include "cwpch.h"

#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    constexpr const char HEX_TO_LITERAL[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    constexpr const uint8_t LITERAL_TO_HEX[256] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF
    };

    UUID42 UUID42::EMPTY = { 0, 0, 0, 0 };

    UUID42::UUID42(uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4) : m_Data{ data1, data2, data3, data4 } {}

    UUID42::UUID42(const String& uuid)
    {
        CW_ENGINE_ASSERT(uuid.size() == 36);
        uint32_t idx = 0;

        for (int32_t i = 7; i >= 0; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[0] |= hex << (i * 4);
        }

        idx++;
        for (int32_t i = 7; i >= 4; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[1] |= hex << (i * 4);
        }

        idx++;
        for (int32_t i = 3; i >= 0; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[1] |= hex << (i * 4);
        }

        idx++;
        for (int32_t i = 7; i >= 4; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[2] |= hex << (i * 4);
        }

        idx++;
        for (int32_t i = 3; i >= 0; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[2] |= hex << (i * 4);
        }

        for (int32_t i = 7; i >= 0; i--)
        {
            uint8_t hex = LITERAL_TO_HEX[(int)uuid[idx++]];
            m_Data[3] |= hex << (i * 4);
        }
    }

    String UUID42::ToString() const
    {
        uint8_t out[36];
        uint32_t idx = 0;

        for (int32_t i = 7; i >= 0; i--)
        {
            uint32_t hex = (m_Data[0] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        out[idx++] = '-';
        for (int32_t i = 7; i >= 4; i--)
        {
            uint32_t hex = (m_Data[1] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        out[idx++] = '-';
        for (int32_t i = 3; i >= 0; i--)
        {
            uint32_t hex = (m_Data[1] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        out[idx++] = '-';
        for (int32_t i = 7; i >= 4; i--)
        {
            uint32_t hex = (m_Data[2] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        out[idx++] = '-';
        for (int32_t i = 3; i >= 0; i--)
        {
            uint32_t hex = (m_Data[2] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        for (int32_t i = 7; i >= 0; i--)
        {
            uint32_t hex = (m_Data[3] >> (i * 4)) & 0xF;
            out[idx++] = HEX_TO_LITERAL[hex];
        }

        return String((const char*)out, 36);
    }

    UUID42 UuidGenerator::Generate() { return PlatformUtils::GenerateUUID(); }

} // namespace Crowny