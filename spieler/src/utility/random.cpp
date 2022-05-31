#include "spieler/config/bootstrap.hpp"

#include "spieler/utility/random.hpp"

namespace spieler
{ 

    static Random g_Instance;

    Random& Random::GetInstance()
    {
        return g_Instance;
    }

    Random::Random()
    {
        const std::seed_seq seedSequence
        {
            std::random_device()(),
            static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
        };

        m_MersenneTwisterEngine = std::mt19937_64(seedSequence);
    }

} // namespace spieler