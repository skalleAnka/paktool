#ifndef PK3_PACK_H_INCLUDED
#define PK3_PACK_H_INCLUDED
#include "../pack.h"
#include <fstream>
#include <minizip/unzip.h>
#include <minizip/zip.h>

namespace pak_impl
{
    class pk3_pack_c : public pak::pack_i
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
        unzFile m_zin = nullptr;
        zipFile m_zout = nullptr;

        static ZCALLBACK ZPOS64_T ztell(void* opaque, void* stream);
        static ZCALLBACK long zseek(void* opaque, void* stream, ZPOS64_T offset, int origin);
        static ZCALLBACK void* zopen(void* opaque, const void* filename, int mode);
        static ZCALLBACK uLong zread(void* opaque, void* stream, void* buf, uLong sz);
        static ZCALLBACK uLong zwrite(void* opaque, void* stream, const void* buf, uLong sz);
        static ZCALLBACK int zclose(void* opaque, void* stream);
        static ZCALLBACK int zerror(void* opaque, void* stream);

        zlib_filefunc64_def m_funcdef
        {
            .zopen64_file = &pk3_pack_c::zopen,
            .zread_file = &pk3_pack_c::zread,
            .zwrite_file = &pk3_pack_c::zwrite,
            .ztell64_file = &pk3_pack_c::ztell,
            .zseek64_file = &pk3_pack_c::zseek,
            .zclose_file = &pk3_pack_c::zclose,
            .zerror_file = &pk3_pack_c::zerror,
            .opaque = this
        };
        
        struct entry_t
        {
            unz64_file_pos pos = { 0ULL, 0ULL };
            uint64_t len = 0ULL;
            std::wstring name;
            std::optional<filetime_t> ts;
        };
        std::vector<entry_t> m_files;
    };
}
#endif
