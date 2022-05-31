#pragma once

namespace spieler
{

    template <typename T>
    class Bits
    {
    private:
        using UnderlyingType = std::underlying_type<T>;

    public:
        bool IsSet(T bit) const;
        void Set(T bit, bool value);

    private:
        std::bitset<sizeof(UnderlyingType)> m_Bits;
    };

} // namespace spieler

#include "bits.inl"