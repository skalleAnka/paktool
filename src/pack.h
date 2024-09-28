#ifndef PACK_H_INCLUDED
#define PACK_H_INCLUDED
#include <string>
#include <memory>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <ranges>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

namespace pak
{
    class pack_i
    {
    public:
        using warning_func_t = std::function<void(std::wstring, std::wstring)>;
        using filetime_t = boost::posix_time::ptime;

        enum class mode { read_only, read_write, rw_new };
    protected:
        pack_i()
        {
        }
        //Implement these and add creation to open_pack/create_pack to add a pack format
        virtual bool open_pack_impl(const std::filesystem::path& path, bool w) = 0;
        virtual bool create_pack_impl(const std::filesystem::path& path) = 0;
        virtual bool open_entry_impl(size_t idx) = 0;
        virtual std::optional<filetime_t> entry_timestamp_impl(size_t idx) const = 0;
        virtual std::optional<size_t> new_entry_impl(const std::wstring& name, const std::optional<filetime_t>& ft) = 0;
        virtual size_t read_entry_impl(std::uint8_t* buf, size_t sz) = 0;
        virtual size_t write_entry_impl(const std::uint8_t* buf, size_t size) = 0;
        virtual void close_write_impl() = 0;
        virtual void close_read_impl() = 0;
        virtual size_t max_filename_len_impl() const = 0;
        virtual size_t max_file_count() const = 0;
        virtual bool close_pack_impl() = 0;
        virtual size_t entry_count() const = 0;
        virtual const std::wstring& entry_name(size_t idx) const = 0;

        virtual bool next_output();

        void emit_warning(const std::wstring& entry, const std::wstring& message)
        {
            if (m_warn_func)
                m_warn_func(entry, message);
        }

        std::optional<size_t> find_entry(const std::wstring& name) const;

        bool m_opened_write = false;
        std::optional<size_t> m_read_idx, m_write_idx;
        std::filesystem::path m_filepath;
    public:

        virtual ~pack_i() = default;

        bool new_entry(const std::wstring& name, const std::optional<filetime_t>& ft = {});
        bool open_entry(const std::wstring& name);
        bool contains_entry(const std::wstring& name) const
        {
             return find_entry(name).has_value();
        }
        std::optional<filetime_t> entry_timestamp() const
        {
            return m_read_idx ? entry_timestamp_impl(*m_read_idx) : std::nullopt;
        }

        void close_read_entry();
        void close_write_entry();

        size_t read(std::uint8_t* data, size_t sz);
        size_t write(const std::uint8_t* data, size_t sz);

        bool close_pack();
        size_t count() const noexcept
        {
            return m_file_idx.size();
        }

        auto file_names() const
        {
            return m_file_idx
                | std::views::transform([this](auto v) { return std::wstring_view{ entry_name(v) }; }); 
        }

        static std::unique_ptr<pack_i> open_pack(const std::filesystem::path& path, mode m, warning_func_t warn_func = nullptr);
    private:
        warning_func_t m_warn_func;
        std::vector<size_t> m_file_idx;

        void rebuild_idx()
        {
            std::vector<size_t> file_idx;
            file_idx.reserve(entry_count());
            std::ranges::copy(std::views::iota(size_t(0)) | std::views::take(entry_count()), back_inserter(file_idx));
            std::ranges::sort(file_idx, {}, [this](auto v) { return boost::to_lower_copy(entry_name(v)); });
            m_file_idx = std::move(file_idx);
        }
    };
}
 #endif

