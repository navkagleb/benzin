#include "benzin/config/bootstrap.hpp"
#include "benzin/utility/string_utils.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    static size_t GetNarrowSize(std::wstring_view wideString)
    {
        const auto wideSize = (int)wideString.size();
        return ::WideCharToMultiByte(CP_UTF8, 0, wideString.data(), wideSize, nullptr, 0, nullptr, nullptr);
    }

    static size_t GetWideSize(std::string_view narrowString)
    {
        const auto narrowSize = (int)narrowString.size();
        return ::MultiByteToWideChar(CP_UTF8, 0, narrowString.data(), narrowSize, nullptr, 0);
    }

    //

    std::string ToNarrowString(std::wstring_view wideString)
    {
        if (wideString.empty())
        {
            return {};
        }

        const auto wideSize = (int)wideString.size();
        const size_t narrowSize = GetNarrowSize(wideString);

        std::string narrow;
        narrow.resize(narrowSize);

        ::WideCharToMultiByte(CP_UTF8, 0, wideString.data(), wideSize, narrow.data(), (int)narrowSize, nullptr, nullptr);

        return narrow;
    }

    std::wstring ToWideString(std::string_view narrowString)
    {
        if (narrowString.empty())
        {
            return {};
        }

        const auto narrowSize = (int)narrowString.size();
        const size_t wideSize = GetWideSize(narrowString);

        std::wstring wide;
        wide.resize(wideSize);

        ::MultiByteToWideChar(CP_UTF8, 0, narrowString.data(), narrowSize, wide.data(), (int)wideSize);

        return wide;
    }

    uint64_t ToU64(std::string_view integerString)
    {
        // Ref: https://jsteemann.github.io/blog/2016/06/02/fastest-string-to-uint64-conversion-method/

        uint64_t result = 0;

        for (const char digit : integerString)
        {
            BenzinAssert('0' <= digit && digit <= '9');
            result = (result << 1) + (result << 3) + digit - '0';
        }

        return result;
    }

    bool IsStringContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty())
        {
            return false;
        }

        return std::ranges::contains_subrange(haystack, needle, [](char lhs, char rhs)
        {
            return std::tolower(lhs) == std::tolower(rhs);
        });
    }

} // namespace benzin
