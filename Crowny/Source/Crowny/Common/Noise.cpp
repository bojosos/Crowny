#include "cwpch.h"
#include "noise.h"

namespace Crowny
{
	float Noise::Round(const glm::vec2& coords)
	{
		auto bump = [](float t) { return glm::max(0.0f, 1.0f - std::pow(t, 6.0f)); };
		float b = bump(coords.x) * bump(coords.y);
		return b * 0.9f;
	}

	float Noise::Noise2D(const NoiseOptions& ops, float xPos, float yPos)
	{
		float val = 0;
		float accAmps = 0;
		for (int i = 0; i < ops.Octaves; i++)
		{
			float freq = glm::pow(2.0f, i);
			float amp = glm::pow(ops.Roughness, i);

			float x = xPos * freq / ops.Smoothness;
			float y = yPos * freq / ops.Smoothness;

			float noise = 0.0f;
			if (ops.NoiseFunc == NoiseFunc::PERLIN)
			{
				noise = glm::perlin(glm::vec3(ops.Seed + x, ops.Seed + y, ops.Seed));
			}
			else
			{
				noise = glm::simplex(glm::vec3(ops.Seed + x, ops.Seed + y, ops.Seed));
			}

			noise = (noise + 1.0f) / 2.0f;
			val += noise * amp;
			accAmps += amp;
		}

		return val / accAmps;
	}

	float Noise::Noise2D(const NoiseOptions& ops, const glm::vec2& position)
	{
		return Noise2D(ops, position.x, position.y);
	}

	float Noise::Noise3D(const NoiseOptions& ops, const glm::vec3& position)
	{
		CW_ENGINE_ASSERT(false, "Not implemented");
		return 0.0f;
	}
}