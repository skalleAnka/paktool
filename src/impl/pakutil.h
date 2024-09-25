#ifndef PAKUTIL_H_INCLUDED
#define PAKUTIL_H_INCLUDED
#include <ranges>

namespace pak_impl
{
    constexpr bool is_ascii(const std::ranges::forward_range auto& str) noexcept
    requires std::is_convertible<std::iter_value_t<decltype(str)>, int>::value
    {
        return std::ranges::find_if(str,
            [](auto v) { return v <= 0 || v > 127; },
            [](auto v) { return static_cast<int>(v); }) == end(str);
    }

    constexpr bool has_ctrl_chars(const std::ranges::forward_range auto& str) noexcept
    requires std::is_convertible<std::iter_value_t<decltype(str)>, int>::value
    {
        return std::ranges::find_if(str,
            [](auto v) { return v < 32; },
            [](auto v) { return static_cast<int>(v); }) != end(str);
    }
}

#endif
