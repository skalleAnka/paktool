#include "fs_pack.h"
#include <ranges>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/core/ignore_unused.hpp>
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
            | views::filter([](const auto& v) { return fs::is_directory(v); }));
    }

    auto path_proj = [](const auto& v)
    {
        return v.path;
    };
}

namespace pak_impl
{
    bool fs_pack_c::open_pack_impl(const fs::path& path, bool w)
    {
        boost::ignore_unused(w);

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
            m_base_path = path;
            return true;
        }
        return false;
    }

    bool fs_pack_c::open_entry_impl(size_t idx)
    {
        m_infile.open(m_files[idx].syspath, ios::in | ios::binary);
        return m_infile.is_open();
    }

    optional<pak::pack_i::filetime_t> fs_pack_c::entry_timestamp_impl(size_t idx) const
    {
        using namespace chrono;
        return time_point_cast<seconds>(clock_cast<system_clock>(fs::last_write_time(m_files[idx].syspath)));
    }
    
    optional<size_t> fs_pack_c::new_entry_impl(const wstring& name, const std::optional<filetime_t>& ft)
    {
        auto parts = name | views::split(L'/')
            | views::transform([](const auto& v) { return wstring(begin(v), end(v)); });
        const vector path_parts(begin(parts), end(parts));

        constexpr wchar_t sep[] = { static_cast<wchar_t>(fs::path::preferred_separator), L'\0' };

        const auto idx = m_files.size();
        m_files.emplace_back();
        m_files.back().path = to_lower_copy(name);
        m_files.back().syspath = boost::join(path_parts, sep);

        m_outfile.open(m_files.back().syspath, ios::binary);
        if (m_outfile.is_open())
        {
            m_pending_ft = ft;
            return idx;
        }
        
        m_files.pop_back();
        return {};
    }
    
    size_t fs_pack_c::read_entry_impl(uint8_t* buf, size_t sz)
    {
        if (m_infile.is_open())
        {
            m_infile.read(reinterpret_cast<char*>(buf), sz);
            return static_cast<size_t>(m_infile.gcount());
        }
        return 0;
    }

    size_t fs_pack_c::write_entry_impl(const std::uint8_t* buf, size_t size)
    {
        if (m_outfile.is_open())
        {
            m_outfile.write(reinterpret_cast<const char*>(buf), size);
            if (m_outfile.fail())
                return 0;

            return size;
        }
        return 0;
    }
    
    void fs_pack_c::close_read_impl()
    {
        if (m_infile.is_open())
            m_infile.close();
    }

    void fs_pack_c::close_write_impl()
    {
        if (m_outfile.is_open())
        {
            m_outfile.close();
            if (m_pending_ft)
                fs::last_write_time(m_files[*m_write_idx].syspath, chrono::file_clock::from_sys(*m_pending_ft));
            m_pending_ft.reset();
        }
    }

    bool fs_pack_c::close_pack_impl()
    {
        close_read_impl();
        close_write_impl();
        m_files.clear();
        return true;
    }

    size_t fs_pack_c::max_filename_len_impl() const
    {
        return PATH_MAX;
    }

    size_t fs_pack_c::max_filename_count() const
    {
        return numeric_limits<size_t>::max();
    }

    size_t fs_pack_c::entry_count() const
    {
        return m_files.size();
    }
    
    const wstring& fs_pack_c::entry_name(size_t idx) const
    {
        return m_files[idx].path;
    }
}
