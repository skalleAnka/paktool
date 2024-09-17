#include "fs_pack.h"
#include <ranges>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#ifndef _WIN32
#include <limits.h>
#endif

namespace fs = std::filesystem;
using namespace std;
using boost::to_lower_copy;

namespace
{
    auto rel_path_make(const fs::path& path, const fs::path& base_path)
    {
        auto subpath = path
            | views::drop(distance(begin(base_path), end(base_path)))
            | views::transform([](const auto& v) { return to_lower_copy(v.wstring()); });
        
        return boost::join(vector(begin(subpath), end(subpath)), L"/");
    }

    auto list_dir_contents(const fs::path& dir, const fs::path base_path)
    {
        return make_tuple(fs::directory_iterator(dir)
            | views::transform([](const auto& v) { return fs::path(v); })
            | views::filter([](const auto& v){ return fs::is_regular_file(v); })
            | views::transform([&](const auto& v) { return make_tuple(v, fs::file_size(v), rel_path_make(v, base_path)); }),
            fs::directory_iterator(dir)
            | views::transform([](const auto& v) { return fs::path(v); })
            | views::filter([](const auto& v) { return fs::is_directory(v) && v.filename().wstring() != L".." && v.filename().wstring() != L"."; }));
    }

    auto path_proj = [](const auto& v)
    {
        return v.path;
    };
}

namespace pak_impl
{
    bool fs_pack_c::open_pack_impl(const fs::path& path)
    {
        if (!fs::is_directory(path))
            return false;
        
        m_files.clear();
        m_opened_write = false;
        
        read_contents(path, path);

        ranges::sort(m_files, {}, path_proj);
        m_base_path = path;
        return true;
    }

    void fs_pack_c::read_contents(const fs::path& path, const std::filesystem::path& base_path)
    {
        auto [files, dirs] = list_dir_contents(path, base_path);
    
        ranges::transform(files, back_inserter(m_files),
            [](const auto& v){ return entry_t(v); });
            
        for (const auto& dir : dirs)
            read_contents(dir, base_path);
    }
    
    bool fs_pack_c::create_pack_impl(const fs::path& path)
    {
        m_files.clear();
        if (fs::create_directory(path))
        {
            m_opened_write = true;
            m_base_path = path;
        }
    
        return m_opened_write;
    }

    bool fs_pack_c::open_entry_impl(const wstring& name)
    {
        if (m_opened_write == false)
        {
            const auto entry = to_lower_copy(name);
            if (auto r = ranges::lower_bound(m_files, entry, {}, path_proj);
                r != end(m_files) && r->path == entry)
            {
                m_file.open(r->syspath, ios::in | ios::binary);
                return m_file.is_open();
            }
        }
        return false;
    }
    
    bool fs_pack_c::new_entry_impl(const wstring& name)
    {
        if (m_opened_write)
        {
            const auto entry = to_lower_copy(name);
            if (auto r = ranges::lower_bound(m_files, entry, {}, path_proj);
                r != end(m_files) && r->path == entry)
            {
                emit_warning(name, L"Duplicate entry");
                return false;
            }
            else
            {
                auto parts = name | views::split(L'/')
                    | views::transform([](const auto& v) { return wstring(begin(v), end(v)); });
                const vector path_parts(begin(parts), end(parts));

                constexpr wchar_t sep[] = { fs::path::preferred_separator, L'\0' };

                m_files.emplace_back();
                m_files.back().path = entry;
                m_files.back().syspath = boost::join(path_parts, sep);

                m_file.open(m_files.back().syspath, ios::out | ios::binary);
                if (m_file.is_open())
                    return true;
                m_files.pop_back();
            }
        }
        return false;
    }
    
    size_t fs_pack_c::read_entry_impl(uint8_t* buf, size_t sz)
    {
        if (m_file.is_open() && !m_opened_write)
        {
            m_file.read(reinterpret_cast<char*>(buf), sz);
            return static_cast<size_t>(m_file.gcount());
        }
        return 0;
    }

    size_t fs_pack_c::write_entry_impl(const std::uint8_t* buf, size_t size)
    {
        if (m_file.is_open() && m_opened_write)
        {
            m_file.write(reinterpret_cast<const char*>(buf), size);
            if (m_file.fail())
            {
                return 0;
            }
            return size;
        }
        return 0;
    }
    
    void fs_pack_c::close_entry_impl()
    {
        if (m_file.is_open())
            m_file.close();
    }

    size_t fs_pack_c::max_filename_len_impl() const
    {
        return PATH_MAX;
    }
}
