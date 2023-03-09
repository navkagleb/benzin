#include "benzin/config/bootstrap.hpp"

#include "benzin/utility/string_utils.h"

namespace benzin
{

	namespace
	{

		size_t GetNarrowSize(std::wstring_view wideString)
		{
			const auto wideSize = static_cast<int>(wideString.size());

			return ::WideCharToMultiByte(CP_UTF8, 0, wideString.data(), wideSize, nullptr, 0, nullptr, nullptr);
		}

		size_t GetWideSize(std::string_view narrowString)
		{
			const auto narrowSize = static_cast<int>(narrowString.size());

			return ::MultiByteToWideChar(CP_UTF8, 0, narrowString.data(), narrowSize, nullptr, 0);
		}

	} // anonymous namespace

	std::string ToNarrowString(std::wstring_view wideString)
	{
		const auto wideSize = static_cast<int>(wideString.size());
		const size_t narrowSize = GetNarrowSize(wideString);

		std::string narrow;
		narrow.resize(narrowSize);

		::WideCharToMultiByte(CP_UTF8, 0, wideString.data(), wideSize, narrow.data(), static_cast<int>(narrowSize), nullptr, nullptr);

		return narrow;
	}

	std::wstring ToWideString(std::string_view narrowString)
	{
		const auto narrowSize = static_cast<int>(narrowString.size());
		const size_t wideSize = GetWideSize(narrowString);

		std::wstring wide;
		wide.resize(wideSize);

		::MultiByteToWideChar(CP_UTF8, 0, narrowString.data(), narrowSize, wide.data(), static_cast<int>(wideSize));

		return wide;
	}

} // namespace benzin
