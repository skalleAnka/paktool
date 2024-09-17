#ifndef FS_PACK_H_INCLUDED
#define FS_PACK_H_INCLUDED
#include "../pack.h"
#include <vector>
#include <fstream>

namespace pak_impl
{
    class fs_pack_c : public pak::pack_i
    {
    public:
        bool open_pack_impl(const std::filesystem::path& path) override;
        virtual bool create_pack_impl(const std::filesystem::path& path) override;
        virtual bool open_entry_impl(const std::wstring& name) override;
        virtual bool new_entry_impl(const std::wstring& name) override;
        virtual size_t read_entry_impl(std::uint8_t* buf, size_t sz) override;
        virtual size_t write_entry_impl(const std::uint8_t* buf, size_t size) override;
        virtual void close_entry_impl() override;
        virtual size_t max_filename_len_impl() const override;
    private:
        struct entry_t
        {
            std::filesystem::path syspath;
            std::int64_t size = 0LL;
            std::wstring path;
            entry_t() = default;
            entry_t(const std::tuple<std::filesystem::path, std::int64_t, std::wstring>& e)
                :syspath(std::get<0>(e)), size(std::get<1>(e)), path(std::get<2>(e))
            {
            }
        };
        std::vector<entry_t> m_files;
        bool m_opened_write = false;
        std::filesystem::path m_base_path;
        
        std::fstream m_file; 

        void read_contents(const std::filesystem::path& path, const std::filesystem::path& base_path);
    };
}

#endif
