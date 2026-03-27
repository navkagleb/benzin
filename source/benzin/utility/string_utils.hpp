#pragma once

namespace benzin
{

    template <typename... Args>
    std::string ArgsToFormatString(std::format_string<Args...> format = "", Args&&... args)
    {
        return std::format(format, std::forward<Args>(args)...);
    }

    std::string ToNarrowString(std::wstring_view wideString);
    std::wstring ToWideString(std::string_view narrowString);

    uint64_t ToU64(std::string_view integerString);

    bool IsStringContainsCaseInsensitive(std::string_view haystack, std::string_view needle);

    std::vector<std::string_view> SplitStringView(std::string_view str, char delimiter);

}
