#ifndef PACK_H_INCLUDED
#define PACK_H_INCLUDED
#include <string>
#include <memory>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <chrono>

namespace pak
{
    class pack_i
    {
    public:
        using warning_func_t = std::function<void(std::wstring, std::wstring)>;
        using filetime_t = std::chrono::sys_time<std::chrono::seconds>;

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
        virtual std::optional<size_t> find_entry(const std::wstring& name) const = 0;
        virtual size_t read_entry_impl(std::uint8_t* buf, size_t sz) = 0;
        virtual size_t write_entry_impl(const std::uint8_t* buf, size_t size) = 0;
        virtual void close_write_impl() = 0;
        virtual void close_read_impl() = 0;
        virtual size_t max_filename_len_impl() const = 0;
        virtual size_t max_filename_count() const = 0;
        virtual bool close_pack_impl() = 0;
        virtual size_t entry_count() const = 0;
        virtual const std::wstring& entry_name(size_t idx) const = 0;

        void emit_warning(const std::wstring& entry, const std::wstring& message)
        {
            if (m_warn_func)
                m_warn_func(entry, message);
        }

        bool m_opened_write = false;
        std::optional<size_t> m_read_idx, m_write_idx;
        std::filesystem::path m_filepath;
    public:

        virtual ~pack_i() = default;

        bool new_entry(const std::wstring& name, const std::optional<filetime_t>& ft = {});
        bool open_entry(const std::wstring& name);
        std::optional<filetime_t> entry_timestamp() const
        {
            return m_read_idx ? entry_timestamp_impl(*m_read_idx) : std::nullopt;
        }

        void close_read_entry();
        void close_write_entry();

        bool close_pack();

        static std::unique_ptr<pack_i> open_pack(const std::filesystem::path& path, mode m, warning_func_t warn_func = nullptr);
    private:
        warning_func_t m_warn_func;
    };
}
 #endif

