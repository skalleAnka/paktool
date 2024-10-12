#include "pak_pack.h"
#include <boost/endian.hpp>
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/core/ignore_unused.hpp>
#include <string_view>
#include <format>
#include "pakutil.h"

namespace fs = std::filesystem;
using namespace std;
using boost::to_lower_copy;
namespace conv = boost::locale::conv;
using namespace boost::endian;

namespace
{
    auto from_text(const string_view& str)
    {
        //Let's hope it's ascii
        if (pak_impl::is_ascii(str))
            return conv::to_utf<wchar_t>(string{ str }, "Latin1");
        
        try
        {
            //Maybe someone stored it as utf-8?
            return conv::utf_to_utf<wchar_t, char>(string{ str });
        }
        catch (const conv::conversion_error& )
        {
        }
        //It must be some legacy encoding and we can't tell which so we guess Win1252
        return conv::to_utf<wchar_t>(string{ str }, "Windows-1252");
    }
}

namespace pak_impl
{
    static constexpr auto PACK = "PACK"sv;

    bool pak_pack_c::open_pack_impl(const fs::path& path, bool w)
    {
        auto oflags = ios::in | ios::binary;
        if (w)
            oflags |= ios::out;

        m_pakfile.open(path, oflags);
        if (!m_pakfile.is_open())
            return false;
        if (!read_header())
        {
            m_files.clear();
            m_pakfile.close();
            return false;
        }
        return true;
    }

    bool pak_pack_c::read_header()
    {
        char buf[PACK.length()];
        if (read_file(m_pakfile, buf) != sizeof(buf) || string_view{ buf, size(buf) } != PACK)
            return false;
        
        const auto ft_offset = little_to_native(read_file<int32_t>(m_pakfile));
        const auto ft_size = little_to_native(read_file<int32_t>(m_pakfile));

        const size_t file_cnt = ft_size / 64u;
        m_files.reserve(file_cnt);

        if (!seek_read(m_pakfile, ft_offset))
            return false;
        
        for (size_t i = 0; i < file_cnt; ++i)
        {
            char buf[56];
            if (read_file(m_pakfile, buf) != sizeof(buf))
                return false;

            m_files.emplace_back(entry_t{
                .pos = little_to_native(read_file<int32_t>(m_pakfile)),
                .len = static_cast<size_t>(little_to_native(read_file<int32_t>(m_pakfile))), 
                .name = from_text(buf)
            });

        }

        return true;
    }

    bool pak_pack_c::create_pack_impl(const fs::path& path)
    {
        m_pakfile.open(path, ios::binary | ios::out | ios::in | ios::trunc);
        if (!m_pakfile.is_open())
            return false;
        
        write_file(m_pakfile, PACK);
        write_file(m_pakfile, int32_t(0));
        write_file(m_pakfile, int32_t(0));
        m_write_offs = m_pakfile.tellp();
        return true;
    }

    bool pak_pack_c::open_entry_impl(size_t idx)
    {
        return seek_write(m_pakfile, m_files[idx].pos);
    }

    optional<pak::pack_i::filetime_t> pak_pack_c::entry_timestamp_impl(size_t idx) const
    {
        //Pak just doesn't support time stamps
        boost::ignore_unused(idx);
        return {};
    }
    
    optional<size_t> pak_pack_c::new_entry_impl(const wstring& name, const optional<filetime_t>& ft)
    {
        boost::ignore_unused(ft);

        const auto idx = m_files.size();
        m_files.emplace_back();
        m_files.back().name = name;
        
        if (seek_write(m_pakfile, m_write_offs))
        {
            m_files.back().pos = m_pakfile.tellp();
            return idx;
        }
        m_files.pop_back();
        return {};
    }

    size_t pak_pack_c::read_entry_impl(uint8_t* buf, size_t sz)
    {
        if (m_pakfile.is_open())
        {
            if (const auto actrd = min(m_files[*m_read_idx].len - m_totread, sz); actrd > 0)
            {
                const auto r = static_cast<size_t>(read_file(m_pakfile, buf, actrd));
                m_totread += r;
                return r;
            }
        }
        return 0;
    }

    size_t pak_pack_c::write_entry_impl(const uint8_t* buf, size_t size)
    {
        if (m_pakfile.is_open())
        {
            write_file(m_pakfile, buf, size);

            m_files[*m_write_idx].len += size;
            if (m_files[*m_write_idx].len > numeric_limits<int32_t>::max())
                throw runtime_error("Entry size too large.");
            return size;
        }
        return 0;
    }

    bool pak_pack_c::close_pack_impl()
    {
        m_files.clear();
        m_pakfile.close();
        return !m_pakfile.is_open();
    }

    void pak_pack_c::close_read_impl()
    {
        m_totread = 0;
    }

    void pak_pack_c::close_write_impl()
    {
        const auto final_pos = static_cast<int64_t>(m_pakfile.tellp());
        const auto dir_size = static_cast<int64_t>(m_files.size() * (sizeof(int32_t) * 2 + 1 + max_filename_len_impl()));

        if (const auto sz = final_pos + dir_size; sz > numeric_limits<int32_t>::max())
            throw runtime_error(format("PAK file size too large ({}) bytes.", sz));
        
        vector<char> namebuf(1 + max_filename_len_impl());
        for (const auto& v : m_files)
        {
            const auto entry = conv::utf_to_utf<char>(v.name);
            if (entry.length() > max_filename_len_impl())
            {
                throw runtime_error(format("Entry name too long: {} (max is {}).",
                    boost::locale::conv::from_utf(v.name, ""), max_filename_len_impl()));
            }

            fill(copy(begin(entry), end(entry), begin(namebuf)), end(namebuf), '\0');
            write_file(m_pakfile, namebuf);
            write_file(m_pakfile, native_to_little(static_cast<int32_t>(v.pos)));
            write_file(m_pakfile, native_to_little(static_cast<int32_t>(v.len)));
        }
        m_pakfile.flush();
        if (!m_pakfile.fail())
        {
            if (seek_write(m_pakfile, PACK.length()))
            {
                write_file(m_pakfile, native_to_little(static_cast<int32_t>(final_pos)));
                write_file(m_pakfile, boost::endian::native_to_little(static_cast<int32_t>(dir_size)));
            }
        }
        m_pakfile.flush();
        if (!seek_write(m_pakfile, final_pos))
            throw runtime_error("Write fail.");
        m_write_offs = final_pos;
    }

    size_t pak_pack_c::max_filename_len_impl() const
    {
        return 55;
    }

    size_t pak_pack_c::max_file_count() const
    {
        return 2048;
    }

    size_t pak_pack_c::entry_count() const
    {
        return m_files.size();
    }

    const wstring& pak_pack_c::entry_name(size_t idx) const
    {
        return m_files[idx].name;
    }
}