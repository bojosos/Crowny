#include "cwpch.h"
#include "Math.h"

namespace Crowny
{
	float Math::Intbound(float s, float ds)
	{
		if (ds < 0.0f)
		{
			return Intbound(-s, -ds);
		}
		else
		{
			s = Mod(s, 1.0f);
			return(1.0f - s) / (float)ds;
		}
	}

	float Math::Signum(float x)
	{
		return x > 0.0f ? 1.0f : x < 0.0f ? -1.0f : 0.0f;
	}

	float Math::Mod(float value, int modulus)
	{
		return (float)fmod((float)fmod(value, modulus) + modulus, modulus);
	}

	glm::vec3 Math::GetForwardDirection(const glm::vec3& rotation)
	{
		float yaw = glm::radians(rotation.y + 90);
		float pitch = glm::radians(rotation.x);
		float x = glm::cos(yaw) * glm::cos(pitch);
		float y = glm::sin(pitch);
		float z = glm::cos(pitch) * glm::sin(yaw);

		return { -x, -y, -z };
	}

	glm::vec3 Math::GetRightDirection(const glm::vec3& rotation)
	{
		float yaw = glm::radians(rotation.y);
		float x = glm::cos(yaw);
		float y = 0;
		float z = glm::sin(yaw);

		return { x, y, z };
	}
}