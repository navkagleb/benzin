#pragma once

namespace benzin
{

    std::string ToNarrowString(std::wstring_view wideString);
    std::wstring ToWideString(std::string_view narrowString);

    uint64_t ToU64(std::string_view integerString);

} // namespace benzin

#define BenzinFormatCstr(formatString, ...) std::format(formatString, __VA_ARGS__).c_str()
