#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{

	class Random : public Module<Random>
	{
		using RandomValueType = std::default_random_engine::result_type;

	public:
		static void Seed(uint32_t seed);
		static float Float();
		static float Float(float min, float max);
		static int32_t Int(int32_t min, int32_t max);

	private:
		virtual void OnInitialize() override;

	private:
		std::default_random_engine m_RandomEngine;
		std::uniform_int_distribution<RandomValueType> m_Distribution;
	};
}