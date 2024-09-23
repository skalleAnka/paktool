#ifndef PAK_PACK_H_INCLUDED
#define PAK_PACK_H_INCLUDED
#include "../pack.h"
#include <fstream>

namespace pak_impl
{
    class pak_pack_c : public pak::pack_i
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
        size_t max_filename_count() const override;
        size_t entry_count() const override;
        const std::wstring& entry_name(size_t idx) const override;
    private:
        std::fstream m_pakfile;
        size_t m_totread = 0;
        std::streamoff m_write_offs = 0;
        
        struct entry_t
        {
            std::streamoff pos = 0;
            std::size_t len = 0;
            std::wstring name;
        };
        std::vector<entry_t> m_files;
    };
}
#endif
