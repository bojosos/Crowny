#pragma once

namespace Crowny
{

	class Random
	{
		using RandomValueType = std::default_random_engine::result_type;

	public:
		static void Init(uint32_t seed = 0);
		static float Float();
		static float Float(float min, float max);
		static int32_t Int(int32_t min, int32_t max);

	private:
		static std::default_random_engine s_RandomEngine;
		static std::uniform_int_distribution<RandomValueType> s_Distribution;
	};
}