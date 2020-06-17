#pragma once

#include <glm/gtc/noise.hpp>

namespace Crowny
{

	enum class NoiseFunc
	{
		PERLIN,
		SIMPLEX,
		VORONOI
	};

	struct NoiseOptions
	{
		int Octaves;
		float Amplitude;
		float Smoothness;
		float Roughness;
		float Offset;
		int Seed;
		NoiseFunc NoiseFunc;
	};

	class Noise
	{
	public:
		static float Round(const glm::vec2& coords);
		static float Noise2D(const NoiseOptions& ops, float xPos, float yPos);
		static float Noise2D(const NoiseOptions& ops, const glm::vec2& position);
		static float Noise3D(const NoiseOptions& ops, const glm::vec3& position);
	};
}