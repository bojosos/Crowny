#include "cwpch.h"

#include "Crowny/Common/Uuid.h"
#include "Crowny/Common/Random.h"

namespace Crowny
{
	Uuid::Uuid()
	{
		m_Uuid = new byte[16];

		for (uint8_t i = 0; i < 16; i++)
		{
			m_Uuid[i] = Random::Int(0, 256);
		}
		m_Uuid[6] &= 0x0f;
		m_Uuid[6] |= 0x40;
		m_Uuid[8] &= 0x3f;
		m_Uuid[8] |= 0x80;
	}

	Uuid::~Uuid()
	{
		delete[] m_Uuid;
	}

	char table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	std::string Uuid::ToString() const
	{
		std::string res;
		for (uint8_t i = 0; i < 16; i++)
		{
			res += table[m_Uuid[i] / 16];
			res += table[m_Uuid[i] % 16];
		}
		return res;
	}

}