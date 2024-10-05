#include "pk3_pack.h"
#include "pakutil.h"
#include <boost/locale.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <array>
#include <tuple>
#include <ranges>

using namespace std;
namespace fs = std::filesystem;

namespace
{
    using ztm_int = decltype(tm_unz::tm_year);  //They kept changing values in time structs around from unsigned to signed

    optional<pak::pack_i::filetime_t> convert_time(const tm_unz& ztime)
    {
        using namespace boost::posix_time;
        using namespace boost::gregorian;
        if (ztime.tm_year < static_cast<ztm_int>(1980))
            return {};
        
        return pak::pack_i::filetime_t
        {
            date{ uint16_t(ztime.tm_year), uint16_t(1 + ztime.tm_mon), uint16_t(ztime.tm_mday) },
            hours(ztime.tm_hour) + minutes(ztime.tm_min) + seconds(ztime.tm_sec)
        };
    }

    auto compression_level(const string& name) noexcept
    {
        auto nmv = name | views::reverse;
        if (auto r = ranges::find(nmv, '.'); r != end(nmv))
        {
            constexpr array uncomp_ext = { "jpg"sv, "jpeg"sv, "png"sv, "mp3"sv, "ogg"sv, "opus"sv, "flac"sv };
            //Skip compression of already compressed files
            const auto ext = string_view{ r.base(), end(name) };
            if (ranges::find(uncomp_ext, ext) != end(uncomp_ext)) 
                return make_tuple(0, Z_NO_COMPRESSION);
        }
        return make_tuple(Z_DEFLATED, Z_BEST_COMPRESSION);
    }
}

namespace pak_impl
{
    //static
    ZCALLBACK ZPOS64_T pk3_pack_c::ztell(void* opaque, void* stream)
    {
        auto p = reinterpret_cast<pk3_pack_c*>(opaque);
        return static_cast<ZPOS64_T>(stream == &p->m_zin ? p->m_pakfile.tellg() : p->m_pakfile.tellp()); 
    }
    //static
    ZCALLBACK long pk3_pack_c::zseek(void* opaque, void* stream, ZPOS64_T offset, int origin)
    {
        constexpr array whence =
        {
            tuple{ ZLIB_FILEFUNC_SEEK_SET, ios::beg },
            tuple{ ZLIB_FILEFUNC_SEEK_CUR, ios::cur },
            tuple{ ZLIB_FILEFUNC_SEEK_END, ios::end }
        };
        
        if (auto r = ranges::find(whence | views::keys, origin).base(); r != end(whence))
        {
            const auto pos = static_cast<streamoff>(offset);
            auto p = reinterpret_cast<pk3_pack_c*>(opaque);
            if (stream == &p->m_zin)
                p->m_pakfile.seekg(pos, get<1>(*r));
            else
                p->m_pakfile.seekp(pos, get<1>(*r));
            
            return p->m_pakfile.fail() ? -1 : 0;
        }
        return -1;
    }
    //static
    ZCALLBACK void* pk3_pack_c::zopen(void* opaque, const void* filename, int mode)
    {
        const auto path = fs::path(reinterpret_cast<const wchar_t*>(filename));
        auto p = reinterpret_cast<pk3_pack_c*>(opaque);

        auto m = ios::binary;
        if (mode & ZLIB_FILEFUNC_MODE_CREATE)
            m |= ios::trunc;
        if ((mode & ZLIB_FILEFUNC_MODE_EXISTING) || p->m_opened_write)
            m |= ios::out;
        if (mode & ZLIB_FILEFUNC_MODE_READ)
            m |= ios::in;

        if (!p->m_pakfile.is_open())
        {            
            p->m_pakfile.open(path, m);
            if (!p->m_pakfile.is_open())
                return nullptr;
        }
        else
        {
            if (mode & ios::in)
                p->m_pakfile.seekg(0, ios::beg);
            if (mode & ios::out)
                p->m_pakfile.seekp(0, ios::beg);
        }
        return (mode & (ZLIB_FILEFUNC_MODE_EXISTING | ZLIB_FILEFUNC_MODE_CREATE)) ? &p->m_zout : &p->m_zin;
    }
    //static
    ZCALLBACK uLong pk3_pack_c::zread(void* opaque, void* stream, void* buf, uLong sz)
    {
        boost::ignore_unused(stream);

        auto p = reinterpret_cast<pk3_pack_c*>(opaque);
        p->m_pakfile.read(reinterpret_cast<char*>(buf), static_cast<streamsize>(sz));
        if (p->m_pakfile.fail())
            return 0;
        return static_cast<uLong>(p->m_pakfile.gcount());
    }
    //static
    ZCALLBACK uLong pk3_pack_c::zwrite(void* opaque, void* stream, const void* buf, uLong sz)
    {
        boost::ignore_unused(stream);
        
        auto p = reinterpret_cast<pk3_pack_c*>(opaque);
        const auto pos = p->m_pakfile.tellp();
        p->m_pakfile.write(reinterpret_cast<const char*>(buf), static_cast<streamsize>(sz));
        if (p->m_pakfile.fail())
            return 0;
        
        return static_cast<uLong>(p->m_pakfile.tellp() - pos);
    }
    //static
    ZCALLBACK int pk3_pack_c::zclose(void* opaque, void* stream)
    {
        boost::ignore_unused(stream);

        auto p = reinterpret_cast<pk3_pack_c*>(opaque);
        if (p->m_zin == nullptr && p->m_zout == nullptr)
            p->m_pakfile.close();
        return 0;
    }
    //static
    ZCALLBACK int pk3_pack_c::zerror(void* opaque, void* stream)
    {
        boost::ignore_unused(stream);

        auto p = reinterpret_cast<pk3_pack_c*>(opaque);
        return p->m_pakfile.fail() ? Z_ERRNO : 0;
    }

    bool pk3_pack_c::open_pack_impl(const std::filesystem::path& path, bool w)
    {
        m_files.clear();
        m_zin = unzOpen2_64(path.wstring().c_str(), &m_funcdef);
        if (m_zin == nullptr)
            return false;
        
        vector<char> filename(1 + 0xFFFF);
        auto cancelret = [this]()
        {
            unzClose(m_zin);
            m_zin = nullptr;
            return false;
        };
        
        for (auto r = unzGoToFirstFile(m_zin); ; r = unzGoToNextFile(m_zin))
        {
            if (r == UNZ_END_OF_LIST_OF_FILE)
                return true;
            else if (r != UNZ_OK)
                return cancelret();
            
            unz_file_info64 info;
            if (unzGetCurrentFileInfo64(m_zin, &info, filename.data(), static_cast<uLong>(filename.size()), nullptr, 0u, nullptr, 0u) != UNZ_OK)
                return cancelret();

            m_files.emplace_back();
            m_files.back().len = info.uncompressed_size;

            m_files.back().name = (info.flag & (1u << 11))
                ? boost::locale::conv::utf_to_utf<wchar_t, char>(filename.data())
                : boost::locale::conv::to_utf<wchar_t>(filename.data(), "CP437");
            
            if (m_files.back().name.ends_with('/'))
            {
                m_files.pop_back();
            }
            else
            {
                m_files.back().ts = convert_time(info.tmu_date);

                if (unzGetFilePos64(m_zin, &m_files.back().pos) != UNZ_OK)
                    return cancelret();
            }
        }

        if (w)
        {
            const auto filename = boost::locale::conv::from_utf(path.wstring(), "");
            m_zout = zipOpen2_64(filename.c_str(), APPEND_STATUS_ADDINZIP, nullptr, &m_funcdef);
            if (m_zout == nullptr)
                return cancelret();
        }
        return true;
    }

    bool pk3_pack_c::create_pack_impl(const fs::path& path)
    {
        m_zout = zipOpen2_64(path.wstring().c_str(), APPEND_STATUS_CREATE, nullptr, &m_funcdef);
        
        return m_zout != nullptr;
    }

    bool pk3_pack_c::open_entry_impl(size_t idx)
    {
        return m_zin
            && unzGoToFilePos64(m_zin, &m_files[idx].pos) == UNZ_OK
            && unzOpenCurrentFile(m_zin) == Z_OK;
    }

    std::optional<pak::pack_i::filetime_t> pk3_pack_c::entry_timestamp_impl(size_t idx) const
    {
        return m_files[idx].ts;
    }

    optional<size_t> pk3_pack_c::new_entry_impl(const wstring& name, const std::optional<filetime_t>& ft)
    {
        static constexpr auto utf8_filename_flag = 1u << 11;
        const auto filename = boost::locale::conv::utf_to_utf<char, wchar_t>(name);
        
        const auto ts = ft.has_value() ? *ft : boost::posix_time::second_clock::local_time();
        
        const zip_fileinfo zfi
        {
            .tmz_date =
            {
                .tm_sec = static_cast<ztm_int>(ts.time_of_day().seconds()),
                .tm_min = static_cast<ztm_int>(ts.time_of_day().minutes()),
                .tm_hour = static_cast<ztm_int>(ts.time_of_day().hours()),
                .tm_mday = static_cast<ztm_int>(ts.date().day()),
                .tm_mon = static_cast<ztm_int>(ts.date().month() - 1u),
                .tm_year = static_cast<ztm_int>(ts.date().year())
            },
            .dosDate = 0u, .internal_fa = 0u, .external_fa = is_ascii(filename) ? 0u : utf8_filename_flag
        };
        
        const auto [method, level] = compression_level(filename);
        if (zipOpenNewFileInZip64(m_zout, filename.c_str(), &zfi, nullptr, 0u,
            nullptr, 0u, nullptr, method, level, 0) == ZIP_OK)
        {
            m_files.emplace_back(entry_t{ .name = name, .ts = {} });
            return m_files.size() -1;
        }
        return {};
    }

    size_t pk3_pack_c::read_entry_impl(std::uint8_t* buf, size_t sz)
    {
        if (m_zin == nullptr || sz > numeric_limits<int>::max())
            return 0;

        if (const auto r = unzReadCurrentFile(m_zin, buf, static_cast<unsigned>(sz)); r >= 0)
        {
            return static_cast<size_t>(r);
        }
        return 0;
    }
    
    size_t pk3_pack_c::write_entry_impl(const std::uint8_t* buf, size_t size)
    {
        if (m_zout == nullptr || size > numeric_limits<unsigned>::max())
            return 0;

        if (zipWriteInFileInZip(m_zout, buf, static_cast<unsigned>(size)) == ZIP_OK)
            return size;
        
        return 0;
    }

    void pk3_pack_c::close_read_impl()
    {
        if (m_zin)
            unzCloseCurrentFile(m_zin);
    }

    void pk3_pack_c::close_write_impl()
    {
        if (m_zout)
        {
            zipCloseFileInZip(m_zout);
            if (m_zin)
            {
                close_read_impl();
                if (!open_pack_impl(m_filepath, false))
                    throw runtime_error("Reopen failed.");
            }
        }
    }

    bool pk3_pack_c::close_pack_impl()
    {
        if (m_zin)
        {
            close_read_impl();
            m_zin = nullptr;
        }
        if (m_zout)
        {
            close_write_impl();
            zipClose(m_zout, nullptr);
            m_zout = nullptr;
        }
        m_pakfile.close();
        return true;
    }

    size_t pk3_pack_c::entry_count() const
    {
        return m_files.size();
    }

    const wstring& pk3_pack_c::entry_name(size_t idx) const
    {
        return m_files[idx].name;
    }

    size_t pk3_pack_c::max_filename_len_impl() const
    {
        return numeric_limits<uint16_t>::max() - 1;
    }
    
    size_t pk3_pack_c::max_file_count() const
    {
        return numeric_limits<uint32_t>::max();
    }
};