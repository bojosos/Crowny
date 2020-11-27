#pragma once

#include <spdlog/fmt/ostr.h>

namespace Crowny
{
	// UUID as struct in rfc-4122 or better?
	class Uuid
	{
	public:
		Uuid() = default;
		Uuid(uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4);
		Uuid(const std::string& uuid);

		std::string ToString() const;

		bool operator==(const Uuid& rhs) const
 		{
 		  return m_Data[0] == rhs.m_Data[0] && m_Data[1] == rhs.m_Data[1] && m_Data[2] == rhs.m_Data[2] && m_Data[3] == rhs.m_Data[3];
 		}

 		bool operator!=(const Uuid& rhs) const
 		{
 		  return !(*this == rhs);
 		}
		
		bool operator<(const Uuid& rhs) const
		{
			for(uint32_t i = 0; i < 4; i++)
			{
			if (m_Data[i] < rhs.m_Data[i])
				return true;
			else if (m_Data[i] > rhs.m_Data[i])
				return false;
			}
			return false;
		}

		template<typename OStream>
		friend OStream& operator<<(OStream& os, const Uuid& ms)
		{
			return os << ms.ToString();
		}

		friend struct std::hash<Uuid>;
	private:
		uint32_t m_Data[4] = {0, 0, 0, 0};
	};

	class UuidGenerator
	{
	public:
		static Uuid Generate();
	};
}

namespace std
{
	template<>
	struct hash<Crowny::Uuid>
	{
		size_t operator()(const Crowny::Uuid& uuid) const
		{
			size_t left = ((size_t)uuid.m_Data[0] << 32) | uuid.m_Data[1];
			size_t right = ((size_t)uuid.m_Data[2] << 32) | uuid.m_Data[3];
			return left ^ right;
		}
	};
}