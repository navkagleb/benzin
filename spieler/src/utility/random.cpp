#include "spieler/config/bootstrap.hpp"

#include "spieler/utility/random.hpp"

namespace spieler
{ 

    static const std::seed_seq g_SeedSequence
    {
        std::random_device()(),
        static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    };

    std::mt19937_64 Random::ms_MersenneTwisterEngine
    {
        g_SeedSequence
    };

} // namespace spieler
