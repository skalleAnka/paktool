#include "../pack.h"
#include "fs_pack.h"
#include "pak_pack.h"
#include "pakutil.h"
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <format>

namespace fs = std::filesystem;
using namespace std;

namespace pak
{
    static constexpr auto PAK = L".pak";
    static constexpr auto PK3 = L".pk3";
    //static
    unique_ptr<pack_i> pack_i::open_pack(const fs::path& path, warning_func_t warn_func)
    {
        unique_ptr<pack_i> ppak;
        if (fs::is_directory(path))
        {
            ppak = make_unique<pak_impl::fs_pack_c>();
            if (!ppak->open_pack_impl(path))
                return nullptr;
        }
        else if (const auto ext = path.extension().wstring(); boost::iequals(ext, PAK))
        {
        }
        else if (boost::iequals(ext, PK3))
        {
        }
        if (ppak)
        {
            ppak->m_warn_func = warn_func;
            ppak->m_opened_write = false;
        }
        return ppak;
    }
    //static
    unique_ptr<pack_i> pack_i::create_pack(const std::filesystem::path& path, warning_func_t warn_func)
    {
        unique_ptr<pack_i> ppak;
        if (const auto ext = path.extension().wstring(); boost::iequals(ext, PAK))
        {
            ppak = make_unique<pak_impl::pak_pack_c>();
            if (!ppak->create_pack_impl(path))
                return nullptr;
        }
        else if (boost::iequals(ext, PK3))
        {
        }
        else if (ext.empty())
        {
            ppak = make_unique<pak_impl::fs_pack_c>();
            if (!ppak->create_pack_impl(path))
                return nullptr;
        }

        if (ppak)
        {
            ppak->m_warn_func = warn_func;
            ppak->m_opened_write = true;
        }
        return ppak;
    }

    bool pack_i::new_entry(const wstring& name)
    {
        if (!m_opened_write)
            throw runtime_error("Pack not writeable.");
        
        if (!pak_impl::is_ascii(name))
            emit_warning(name, L"New entry contains non-ASCII characters.");
        
        if (name.length() > max_filename_len_impl())
        {
            const auto str = format(L"File name {} too long. Maximum length is {}.", name, max_filename_len_impl());
            throw runtime_error(boost::locale::conv::from_utf(str, ""));
        }

        if (auto e = find_entry(name))
        {
            emit_warning(name, L"Duplicate entry."s);
            return false;
        }
        m_read_idx = new_entry_impl(name);
        return m_read_idx.has_value();
    }

    bool pack_i::open_entry(const wstring& name)
    {
        if (auto e = find_entry(name); e && open_entry_impl(*e))
        {
            m_read_idx = e;
            return true;
        }
        emit_warning(name, L"Entry not found.");
        return false;
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
    }

    bool pack_i::close_pack()
    {
        return close_pack_impl();
    }
}

