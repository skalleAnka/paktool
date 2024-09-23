#include "pk3_pack.h"
#include <boost/locale.hpp>
#include <boost/core/ignore_unused.hpp>
#include <array>
#include <tuple>
#include <ranges>

using namespace std;
namespace fs = std::filesystem;

namespace
{
    optional<pak::pack_i::filetime_t> convert_time(const tm_unz& ztime)
    {
        using namespace chrono;

        if (ztime.tm_year < 1980)
            return {};
        
        const auto dt = year_month_day{ year(ztime.tm_year), month(1 + ztime.tm_mon), day(ztime.tm_mday) };
        const auto t = hh_mm_ss{ hours(ztime.tm_hour) + minutes(ztime.tm_min) + seconds(ztime.tm_sec) };

        return sys_time<seconds>(sys_days{ dt } + t.to_duration());
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
            if (unzGetCurrentFileInfo64(m_zin, &info, filename.data(), filename.size(), nullptr, 0u, nullptr, 0u) != UNZ_OK)
                return cancelret();

            m_files.emplace_back();
            m_files.back().len = info.uncompressed_size;

            m_files.back().name = (info.flag & (1u << 11))
                ? boost::locale::conv::utf_to_utf<wchar_t, char>(filename.data())
                : boost::locale::conv::to_utf<wchar_t>(filename.data(), "CP437");
            
            m_files.back().ts = convert_time(info.tmu_date);

            if (unzGetFilePos64(m_zin, &m_files.back().pos) != UNZ_OK)
                return cancelret();
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
        const auto filename = boost::locale::conv::from_utf(path.wstring(), "");
        m_zout = zipOpen2_64(filename.c_str(), APPEND_STATUS_CREATE, nullptr, &m_funcdef);
        
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
        const auto filename = boost::locale::conv::utf_to_utf<char, wchar_t>(name);
        
        const auto ts = ft.has_value() ? *ft : chrono::utc_clock::to_sys(chrono::utc_clock::now());
        const auto dt = chrono::year_month_day{ floor<chrono::days>(ts) };
        const auto tod = chrono::hh_mm_ss{ ts.time_since_epoch() };
        
        const zip_fileinfo zfi
        {
            .tmz_date =
            {
                .tm_sec = static_cast<uInt>(tod.seconds().count()),
                .tm_min = static_cast<uInt>(tod.minutes().count()),
                .tm_hour = static_cast<uInt>(tod.hours().count()),
                .tm_mday = static_cast<uInt>(dt.day()),
                .tm_mon = static_cast<uInt>((dt.month())) - 1u,
                .tm_year = static_cast<uInt>(static_cast<int>(dt.year()))   //lmao @ chrono
            },
            .dosDate = 0u, .internal_fa = 0u, .external_fa = 0u
        };
            
        if (zipOpenNewFileInZip64(m_zout, filename.c_str(), &zfi, nullptr, 0u,
            nullptr, 0u, nullptr, Z_DEFLATED, Z_BEST_COMPRESSION, 0) == ZIP_OK)
        {
            m_files.emplace_back(entry_t{ .name = name });
            return m_files.size() -1;
        }
        return {};
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
    
    size_t pk3_pack_c::max_filename_count() const
    {
        return numeric_limits<uint32_t>::max();
    }
};