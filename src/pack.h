#ifndef PACK_H_INCLUDED
#define PACK_H_INCLUDED
#include <string>
#include <memory>
#include <cstdint>
#include <filesystem>
#include <functional>

namespace pak
{
    class pack_i
    {
    protected:
        pack_i()
        {
        }
        //Implement these and add creation to open_pack/create_pack to add a pack format
        virtual bool open_pack_impl(const std::filesystem::path& path) = 0;
        virtual bool create_pack_impl(const std::filesystem::path& path) = 0;
        virtual bool open_entry_impl(const std::wstring& name) = 0;
        virtual bool new_entry_impl(const std::wstring& name) = 0;
        virtual size_t read_entry_impl(std::uint8_t* buf, size_t sz) = 0;
        virtual size_t write_entry_impl(const std::uint8_t* buf, size_t size) = 0;
        virtual void close_entry_impl() = 0;
        virtual size_t max_filename_len_impl() const = 0;

        void emit_warning(const std::wstring& entry, const std::wstring& message)
        {
            if (m_warn_func)
                m_warn_func(entry, message);
        }
    public:
        virtual ~pack_i()
        {
        }

        using warning_func_t = std::function<void(std::wstring, std::wstring)>;

        static std::unique_ptr<pack_i> open_pack(const std::filesystem::path& path, warning_func_t warn_func = nullptr);
        static std::unique_ptr<pack_i> create_pack(const std::filesystem::path& path, warning_func_t warn_func = nullptr);
    private:
        warning_func_t m_warn_func;
    };
}
 #endif

