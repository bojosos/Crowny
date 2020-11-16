#pragma once

#include <spdlog/fmt/ostr.h>

namespace Crowny
{
	// UUID as struct in rfc-4122 or better?
	class Uuid
	{
	public:
		Uuid();
		~Uuid();

		byte* Get() { return m_Uuid; }

		std::string ToString() const;

		operator std::string () const { return ToString(); }

		template<typename OStream>
		friend OStream& operator<<(OStream& os, const Uuid& ms)
		{
			return os << ms.ToString();
		}
	private:
		byte* m_Uuid; // no need for this
	};
}