#pragma once

// Returns byte count
constexpr uint64_t operator"" _kb(uint64_t kb) { return kb * 1024; }
constexpr uint64_t operator"" _mb(uint64_t mb) { return mb * 1024 * 1024; }
constexpr uint64_t operator"" _gb(uint64_t gb) { return gb * 1024 * 1024 * 1024; }

namespace benzin
{

    template <std::integral T>
    class Bytes
    {
    public:
        constexpr Bytes() = default;

        constexpr Bytes(T byteCount)
            : m_ByteCount{ byteCount }
        {}

        template <std::integral U>
        constexpr Bytes(Bytes<U> other)
            : m_ByteCount{ other.GetBytes() }
        {}

        constexpr T GetBytes() const { return m_ByteCount; }
        void SetBytes(T byteCount) { m_ByteCount = byteCount; }

        constexpr float GetKb() const { return (float)m_ByteCount / 1_kb; };
        constexpr float GetMb() const { return (float)m_ByteCount / 1_mb; };
        constexpr float GetGb() const { return (float)m_ByteCount / 1_gb; };

        constexpr operator T() const { return m_ByteCount; }

        T* operator&() { return &m_ByteCount; }

    private:
        T m_ByteCount = 0;
    };

    using Bytes32 = Bytes<uint32_t>;
    using Bytes64 = Bytes<uint64_t>;

}

constexpr benzin::Bytes32 operator"" _bytes32(uint64_t byteCount) { return (uint32_t)byteCount; }
constexpr benzin::Bytes64 operator"" _bytes64(uint64_t byteCount) { return byteCount; }

template <std::integral T, std::integral U>
constexpr auto operator+(benzin::Bytes<T> lhs, benzin::Bytes<U> rhs)
{
    return benzin::Bytes{ lhs.GetBytes() + rhs.GetBytes() };
}

template <std::integral T, std::integral U>
constexpr auto& operator+=(benzin::Bytes<T>& lhs, benzin::Bytes<U> rhs)
{
    lhs.SetBytes(lhs.GetBytes() + rhs.GetBytes());
    return lhs;
}

template <std::integral T, std::integral U>
constexpr auto operator-(benzin::Bytes<T> lhs, benzin::Bytes<U> rhs)
{
    return benzin::Bytes{ lhs.GetBytes() - rhs.GetBytes() };
}

template <std::integral T, std::integral U>
constexpr auto& operator-=(benzin::Bytes<T>& lhs, benzin::Bytes<U> rhs)
{
    lhs.SetBytes(lhs.GetBytes() - rhs.GetBytes());
    return lhs;
}
