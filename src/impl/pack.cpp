#include "../pack.h"
#include "fs_pack.h"
#include "pak_pack.h"
#include "pk3_pack.h"
#include "grp_pack.h"
#include "pakutil.h"
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/core/ignore_unused.hpp>
#include <format>
#include <regex>

namespace fs = std::filesystem;
using namespace std;

namespace
{
    wstring conv_separators(const wstring& str)
    {
        auto filename = str;
        if constexpr (fs::path::preferred_separator != L'/')
            ranges::replace(filename, fs::path::preferred_separator, L'/');

        return filename;
    }
}
namespace pak
{
    static constexpr auto PAK = L".pak";
    static constexpr auto PK3 = L".pk3";
    static constexpr auto ZIP = L".zip";
    static constexpr auto GRP = L".grp";
    //static
    unique_ptr<pack_i> pack_i::open_pack(const fs::path& path, mode m, warning_func_t warn_func)
    {
        unique_ptr<pack_i> ppak;
        if (fs::is_directory(path))
            ppak = make_unique<pak_impl::fs_pack_c>();
        else if (const auto ext = path.extension().wstring(); boost::iequals(ext, PAK))
            ppak = make_unique<pak_impl::pak_pack_c>();
        else if (boost::iequals(ext, PK3) || boost::iequals(ext, ZIP))
            ppak = make_unique<pak_impl::pk3_pack_c>();
        else if (boost::iequals(ext, GRP))
            ppak = make_unique<pak_impl::grp_pack_c>();
        else if (ext.empty() && m == mode::rw_new && fs::is_directory(path.parent_path()))
            ppak = make_unique<pak_impl::fs_pack_c>();

        if (ppak)
        {
            ppak->m_warn_func = warn_func;
            ppak->m_opened_write = m != mode::read_only;
            ppak->m_filepath = path;

            switch (m)
            {
            case mode::rw_new:
                if (!ppak->create_pack_impl(ppak->m_filepath))
                    return nullptr;
                break;
            case mode::read_write: [[fallthrough]];
            case mode::read_only:
                if (!ppak->open_pack_impl(path, ppak->m_opened_write))
                    return nullptr;
                ppak->rebuild_idx();
                break;
            }
        }
        return ppak;
    }

    bool pack_i::notify_add(size_t cnt)
    {
        //Re-implement if the pack needs to allocate space in the file
        //before a large add operation
        boost::ignore_unused(cnt);
        return m_opened_write;
    }

    bool pack_i::next_output()
    {
        const auto name = m_filepath.filename().replace_extension(L"").string();
        const regex re("^pak([0-9]+)$", regex_constants::icase);
        std::smatch m;
        if (regex_search(name, m, re) && m.size() == 2u)
        {
            const auto newnum = boost::lexical_cast<int>(m[1].str()) + 1;

            auto filepath = m_filepath.parent_path()
                / format("{}{}{}", name.substr(0, 3), newnum, m_filepath.filename().extension().string());

            if (!fs::exists(filepath) && close_pack())
            {
                std::swap(filepath, m_filepath);
                if (create_pack_impl(m_filepath))
                    return true;
                std::swap(filepath, m_filepath);
            }
        }

        return false;
    }

    bool pack_i::new_entry(const wstring& name, const optional<filetime_t>& ft)
    {
        if (!m_opened_write)
            throw runtime_error("Pack not writeable.");
        
        if (const auto n = max_file_count(); entry_count() >= n)
        {
            if (!next_output())
                throw runtime_error(format("Maximum file count of {} reached in {}.", n, m_filepath.filename().string()));
        }
        
        const auto filename = conv_separators(name);

        if (!pak_impl::is_ascii(filename))
            emit_warning(filename, L"New entry name contains non-ASCII characters.");
        if (pak_impl::has_ctrl_chars(filename))
            emit_warning(filename, L"New entry name contains control characters.");
        
        if (const auto u8nm = boost::locale::conv::utf_to_utf<char, wchar_t>(filename);
            u8nm.length() > max_filename_len_impl())
        {
            const auto str = format(L"File name {} too long. Maximum length is {}.", name, max_filename_len_impl());
            throw runtime_error(boost::locale::conv::from_utf(str, ""));
        }
        else if (u8nm.length() > 55u)
        {
            emit_warning(filename, format(L"File name {} too long for most Quake engines.", name));
        }

        if (auto e = find_entry(filename))
        {
            emit_warning(name, L"Duplicate entry."s);
            return false;
        }
        m_write_idx = new_entry_impl(name, ft);
        return m_write_idx.has_value();
    }

    bool pack_i::open_entry(const wstring& name)
    {
        const auto filename = conv_separators(name);
        if (auto e = find_entry(filename); e && open_entry_impl(*e))
        {
            m_read_idx = e;
            return true;
        }
        emit_warning(filename, L"Entry not found.");
        return false;
    }

    optional<size_t> pack_i::find_entry(const wstring& name) const
    {
        auto names = m_file_idx
            | views::transform([this](auto v) { return boost::to_lower_copy(entry_name(v)); });

        if (auto r = ranges::lower_bound(names, boost::to_lower_copy(name));
            r != end(names) && boost::iequals(name, *r))
        {
            return *r.base();
        }
        return {};
    }

    size_t pack_i::read(uint8_t* data, size_t sz)
    {
        if (m_read_idx)
            return read_entry_impl(data, sz);
        return 0;
    }

    size_t pack_i::write(const uint8_t* data, size_t sz)
    {
        if (m_write_idx)
            return write_entry_impl(data, sz);
        return 0;
    }

    void pack_i::close_read_entry()
    {
        if (m_read_idx.has_value())
            close_read_impl();
        m_read_idx.reset();
    }
    
    void pack_i::close_write_entry()
    {
        if (m_write_idx.has_value())
            close_write_impl();
        m_write_idx.reset();
        rebuild_idx();
    }

    bool pack_i::close_pack()
    {
        return close_pack_impl();
    }
}
