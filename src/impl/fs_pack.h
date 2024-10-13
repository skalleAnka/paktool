#ifndef FS_PACK_H_INCLUDED
#define FS_PACK_H_INCLUDED
#include "../pack.h"
#include <vector>
#include <fstream>

namespace pak_impl
{
    class fs_pack_c : public pak::pack_i
    {
    protected:
        bool open_pack_impl(const std::filesystem::path& path, bool w) override;
        bool create_pack_impl(const std::filesystem::path& path) override;
        bool open_entry_impl(size_t idx) override;
        std::optional<filetime_t> entry_timestamp_impl(size_t idx) const override;
        std::optional<size_t> new_entry_impl(const std::wstring& name, const std::optional<filetime_t>& ft) override;
        size_t read_entry_impl(std::uint8_t* buf, size_t sz) override;
        size_t write_entry_impl(const std::uint8_t* buf, size_t size) override;
        bool close_pack_impl() override;
        void close_read_impl() override;
        void close_write_impl() override;
        size_t max_filename_len_impl() const override;
        size_t max_file_count() const override;
        size_t entry_count() const override;
        const std::wstring& entry_name(size_t idx) const override;
    private:
        struct entry_t
        {
            std::filesystem::path syspath;
            std::int64_t size = 0LL;
            std::wstring path;
            entry_t() = default;
            explicit entry_t(const std::tuple<std::filesystem::path, std::int64_t, std::wstring>& e)
                : syspath(std::get<0>(e)), size(std::get<1>(e)), path(std::get<2>(e))
            {
            }
        };
        std::vector<entry_t> m_files;
        std::filesystem::path m_base_path;
        std::optional<filetime_t> m_pending_ft;
        
        std::ifstream m_infile;
        std::ofstream m_outfile; 

        void read_contents(const std::filesystem::path& path, const std::filesystem::path& base_path);
    };
}

#endif
