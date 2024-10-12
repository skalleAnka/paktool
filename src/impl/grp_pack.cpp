#include "grp_pack.h"
#include "pakutil.h"
#include <numeric>
#include <regex>
#include <boost/endian.hpp>
#include <boost/locale.hpp>
#include <boost/core/ignore_unused.hpp>

using namespace std;
namespace fs = std::filesystem;
using namespace boost::endian;
namespace conv = boost::locale::conv;

namespace pak_impl
{
    static constexpr auto KEN = "KenSilverman"sv;
    constexpr size_t header_size = 12 + 4;
    constexpr size_t entry_size = 12 + 4;

    bool grp_pack_c::read_header()
    {
        char ken[KEN.length()];
        
        if (read_file(m_pakfile, ken) != KEN.length() || string_view{ begin(ken), end(ken) } != KEN)
            return false;
        
        const auto file_cnt = little_to_native(read_file<uint32_t>(m_pakfile));  
        const auto data_offs = header_size + file_cnt * entry_size;
        
        for (auto i = 0u; i < file_cnt; ++i)
        {
            char filenbuf[12];

            m_pakfile.read(filenbuf, sizeof(filenbuf));
            if (read_file(m_pakfile, filenbuf) != sizeof(filenbuf))
                return false;
            //It should really be ASCII only but who knows with old DOS files
            auto file_name = conv::to_utf<wchar_t>({ begin(filenbuf), ranges::find(filenbuf, '\0') }, "CP437");
            
            const auto filesz = native_to_little(read_file<uint32_t>(m_pakfile));
            const auto offs = i == 0 ? data_offs : m_files.back().pos + m_files.back().len;

            m_files.emplace_back(entry_t{
                .pos = static_cast<streamoff>(offs),
                .len = filesz,
                .name = std::move(file_name)});
        }

        if (!m_files.empty())
        {
            if (!seek_read(m_pakfile, 0, ios::end) || m_pakfile.tellg() != static_cast<streampos>(m_files.back().pos + m_files.back().len))
                return false;
        }
        return true;
    }

    bool grp_pack_c::create_pack_impl(const fs::path& path)
    {
        m_pakfile.open(path, ios::binary | ios::out | ios::in | ios::trunc);
        if (!m_pakfile.is_open())
            return false;
        
        write_file(m_pakfile, KEN);
        write_file<uint32_t>(m_pakfile, 0u);
        return true;
    }

    optional<size_t> grp_pack_c::new_entry_impl(const wstring& name, const optional<filetime_t>& ft)
    {
        boost::ignore_unused(ft);

        if (!m_pending_cnt.has_value())
        {
            if (!notify_add(1))
                return {};
        }

        if (!regex_match(name, wregex{ LR"(^[A-Za-z0-9]{1,8}\.[A-Za-z0-9]{0,3}$)" }))
            emit_warning(name, L"Not a DOS 8.3 file name.");

        const auto idx = m_files.size();
        m_files.emplace_back();
        m_files.back().name = name;
        boost::to_upper(m_files.back().name);
        
        if (seek_write(m_pakfile, 0, ios::end))
        {
            m_files.back().pos = m_pakfile.tellp();
            return idx;
        }
        m_files.pop_back();
        return {};
    }

    void grp_pack_c::close_write_impl()
    {
        m_pending_used = m_pending_used.value_or(0) + 1;
        if (m_pending_used.value() >= m_pending_cnt.value())
        {
            m_pending_cnt.reset();
            m_pending_used.reset();
        }

        if (seek_write(m_pakfile, header_size + entry_size * m_write_idx.value()))
        {
            write_file(m_pakfile, boost::to_upper_copy(conv::from_utf(m_files[*m_write_idx].name, "CP437")));
            write_file(m_pakfile, native_to_little(static_cast<uint32_t>(m_files[*m_write_idx].len)));
        } 
    }

    bool grp_pack_c::close_pack_impl()
    {
        if (m_pending_cnt.has_value())
            return false;

        return pak_pack_c::close_pack_impl();
    }

    bool grp_pack_c::notify_add(size_t cnt)
    {
        if (m_pending_cnt.has_value() && m_pending_used.has_value())
            throw runtime_error("File operations already pending.");

        if (shift_data(m_pending_cnt.value_or(0) + cnt)
            && seek_write(m_pakfile, KEN.length()))
        {
            m_pending_cnt = cnt;
            write_file(m_pakfile, native_to_little(static_cast<uint32_t>(m_files.size() + cnt)));
            return true;
        }
        return false;
    }

    bool grp_pack_c::shift_data(size_t num_files)
    {
        //GRP files unfortunately have the file table before all the data, so adding
        //files requires shifting all the data towards the end
        auto file_sizes = m_files | views::transform([](const auto& v) { return v.len; });
        const auto data_size = accumulate(begin(file_sizes), end(file_sizes), size_t(0));

        if (const auto shiftpos = static_cast<streamsize>(min(num_files * entry_size, data_size)))
        {
            if (seek_read(m_pakfile, -static_cast<streampos>(shiftpos), ios::end))
                return false;
            
            constexpr streamsize bsz = 4096;

            vector<char> buf(max(bsz, shiftpos));

            if (read_file(m_pakfile, buf.data(), shiftpos) != shiftpos)
                return false;
            
            if (!seek_write(m_pakfile, 0, ios::end))
                return false;
            
            write_file(m_pakfile, buf.data(), shiftpos);
            
            for (streampos i = data_size - bsz; i > 0; i -= bsz)
            {
                const auto rsz = min(static_cast<streamsize>(i), bsz);
                if (!seek_read(m_pakfile, i))
                    return false;
            
                if (!seek_write(m_pakfile, m_pakfile.tellg() + shiftpos))
                    return false;
                
                m_pakfile.read(buf.data(), rsz);
                if (read_file(m_pakfile, buf.data(), rsz) != rsz)
                    return false;
                
                write_file(m_pakfile, buf.data(), rsz);
            }
        }

        if (!seek_write(m_pakfile, header_size + m_files.size() * entry_size))
            return false;
        
        vector<char> placeholder(num_files * entry_size);
        ranges::fill(placeholder, '\0');
        write_file(m_pakfile, placeholder);
        return true;
    }

    size_t grp_pack_c::max_filename_len_impl() const
    {
        return 12u;
    }

    size_t grp_pack_c::max_file_count() const
    {
        //It will fail way before this,but it depends on entry sizes more than anything
        return numeric_limits<uint32_t>::max();
    }
}
