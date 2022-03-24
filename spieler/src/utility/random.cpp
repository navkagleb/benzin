#include "utility/random.hpp"

#include <chrono>

namespace Spieler
{ 

    Random& Random::GetInstance()
    {
        static Random instance;
        return instance;
    }

    Random::Random()
    {
        std::seed_seq seedSequence
        {
            std::random_device()(),
            static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
        };

        m_MersenneTwisterEngine = std::mt19937_64(seedSequence);
    }

} // namespace Spieler