#ifndef GRP_PACK_H_INCLUDED
#define GRP_PACK_H_INCLUDED

#include "pak_pack.h"
#include <vector>
#include <fstream>

namespace pak_impl
{
    class grp_pack_c : public pak_pack_c
    {
    protected:
        bool create_pack_impl(const std::filesystem::path& path) override;
        std::optional<size_t> new_entry_impl(const std::wstring& name, const std::optional<filetime_t>& ft) override;
        bool close_pack_impl() override;
        void close_write_impl() override;
        size_t max_filename_len_impl() const override;
        size_t max_file_count() const override;
        bool notify_add(size_t cnt) override;

        bool read_header() override;
    private:
        std::optional<size_t> m_pending_cnt, m_pending_used;
        bool shift_data(size_t num_files);
    };
}

#endif
