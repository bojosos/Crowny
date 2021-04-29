#include "cwpch.h"

#include "Crowny/Common/Random.h"

namespace Crowny
{

	void Random::Seed(uint32_t seed)
	{
		Get().m_RandomEngine.seed(seed ? seed : std::random_device()());
	}
	
	void Random::OnInitialize()
	{
		m_RandomEngine.seed(std::random_device()());
	}

	float Random::Float()
	{
		return (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
	}

	float Random::Float(float min, float max)
	{
		float randf = (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
		return min + (max - min) * randf;
	}

	int32_t Random::Int(int32_t min, int32_t max)
	{
		float randf = (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
		return (int32_t)(min + (max - min) * randf);
	}

}