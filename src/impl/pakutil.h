#ifndef PAKUTIL_H_INCLUDED
#define PAKUTIL_H_INCLUDED
#include <ranges>
#include <fstream>

namespace pak_impl
{
    template <typename T>
    concept pod_type = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

    template <typename Ts>
    concept input_stream = std::is_base_of_v<std::istream, Ts>;

    template <typename Ts>
    concept output_stream = std::is_base_of_v<std::ostream, Ts>;

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

    constexpr bool is_filename(const std::ranges::forward_range auto& str) noexcept
    requires std::is_convertible<std::iter_value_t<decltype(str)>, int>::value
    {
        constexpr std::array forbidden = { '\\', '/', ':', '*', '?', '"', '<', '>', '|' };
        return std::ranges::find_if(str,
            [fb = forbidden | std::ranges::views::transform([](auto c){ return static_cast<int>(c);}) ]
            (auto v) { return std::ranges::find(fb, static_cast<int>(v)) != end(fb); }) == end(str);
    }

    inline void write_file(output_stream auto& file, const void* data, std::streamsize sz)
    {
        file.write(reinterpret_cast<const char*>(data), sz);
        if (file.fail())
            throw std::runtime_error("Write error.");
    }

    template <pod_type T>
    inline void write_file(output_stream auto& file, const T& v)
    {
        write_file(file, &v, sizeof(v));
    }

    template <std::ranges::contiguous_range R>
    requires pod_type<std::ranges::range_value_t<R>>
    inline void write_file(output_stream auto& file, const R& arr)
    {
        return write_file(file, std::ranges::data(arr), sizeof(std::ranges::range_value_t<R>) * std::ranges::size(arr));
    }

    inline std::streamsize read_file(input_stream auto& file, void* data, std::streamsize sz)
    {
        file.read(reinterpret_cast<char*>(data), sz);
        if (file.fail())
            throw std::runtime_error("Read error.");
        return file.gcount();
    }

    template <pod_type T>
    inline void read_file(input_stream auto& file, T& data)
    {
        if (read_file(file, &data, sizeof(T)) != sizeof(T))
            throw std::runtime_error("Read error.");
    }

    template <pod_type T, std::size_t N>
    inline std::streamsize read_file(input_stream auto& file, T (&arr)[N])
    {
        return read_file(file, arr, sizeof(arr));
    }

    template <pod_type T>
    inline T read_file(input_stream auto& file)
    {
        T data;
        read_file(file, data);
        return data;
    }

    inline bool seek_read(input_stream auto& file, std::streampos pos, std::ios_base::seekdir whence = std::ios::beg)
    {
        file.seekg(pos, whence);
        return !file.fail();
    }

    inline bool seek_write(output_stream auto& file, std::streampos pos, std::ios_base::seekdir whence = std::ios::beg)
    {
        file.seekp(pos, whence);
        return !file.fail();
    }
}

#endif
