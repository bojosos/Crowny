#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{

    class Random : public Module<Random>
    {
        using RandomValueType = std::default_random_engine::result_type;

    public:
        /**
         * @brief Initializes the random engine with a seed.
         *
         * @param seed Seed to use.
         */
        static void Seed(uint32_t seed);

        /**
         * @brief Generates a random float number in the range (0, 1).
         *
         * @return Random float
         */
        static float Float();

        /**
         * @brief Generates a random float number in the range (min, max).
         *
         * @param min Minimum value of random number.
         * @param max Maximum value of random number.
         *
         * @return Random float.
         */
        static float Float(float min, float max);

        /**
         * @brief Generates a random integer in the range (min, max).
         *
         * @param min Minimum value of random number.
         * @param max Maximum value of random number.
         * @return Random integer.
         */
        static int32_t Int(int32_t min, int32_t max);

        static glm::vec2 InsideUnitCircle();
        static glm::vec3 InsideUnitSphere();

    private:
        virtual void OnStartUp() override;

    private:
        std::default_random_engine m_RandomEngine;
        std::uniform_int_distribution<RandomValueType> m_Distribution;
    };
} // namespace Crowny