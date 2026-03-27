#pragma once

constexpr uint64_t operator"" _kb(uint64_t kb) { return kb * 1024; }
constexpr uint64_t operator"" _mb(uint64_t mb) { return mb * 1024 * 1024; }
constexpr uint64_t operator"" _gb(uint64_t gb) { return gb * 1024 * 1024 * 1024; }

namespace benzin
{

    inline float ToKb(uint64_t sizeInBytes) { return (float)sizeInBytes / 1_kb; }
    inline float ToMb(uint64_t sizeInBytes) { return (float)sizeInBytes / 1_mb; }
    inline float ToGb(uint64_t sizeInBytes) { return (float)sizeInBytes / 1_gb; }

}
