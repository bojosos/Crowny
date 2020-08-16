#include "cwpch.h"

#include "Crowny/Common/Random.h"

namespace Crowny
{

	std::default_random_engine Random::s_RandomEngine;
	std::uniform_int_distribution<Random::RandomValueType> Random::s_Distribution;

	void Random::Init(uint32_t seed)
	{
		s_RandomEngine.seed(seed ? seed : std::random_device()());
	}

	float Random::Float()
	{
		return (float)s_Distribution(s_RandomEngine) / (float)std::numeric_limits<RandomValueType>::max();
	}

	float Random::Float(float min, float max)
	{
		float randf = (float)s_Distribution(s_RandomEngine) / (float)std::numeric_limits<RandomValueType>::max();
		return min + (max - min) * randf;
	}

	int32_t Random::Int(int32_t min, int32_t max)
	{
		float randf = (float)s_Distribution(s_RandomEngine) / (float)std::numeric_limits<RandomValueType>::max();
		return (int32_t)(min + (max - min) * randf);
	}

}