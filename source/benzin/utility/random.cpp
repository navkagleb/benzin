#include "benzin/config/bootstrap.hpp"
#include "benzin/utility/random.hpp"

namespace benzin
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

} // namespace benzin
