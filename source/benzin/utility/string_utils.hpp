#pragma once

namespace benzin
{

    std::string ToNarrowString(std::wstring_view wideString);
    std::wstring ToWideString(std::string_view narrowString);

} // namespace benzin
