#include "../pack.h"
#include "fs_pack.h"
#include <boost/algorithm/string.hpp>

namespace fs = std::filesystem;

namespace pak
{
    //static
    std::unique_ptr<pack_i> pack_i::open_pack(const fs::path& path, warning_func_t warn_func)
    {
        if (fs::is_directory(path))
        {
            auto ppak = std::make_unique<pak_impl::fs_pack_c>();
            ppak->m_warn_func = warn_func;
            if (ppak->open_pack_impl(path))
                return ppak;
        }
        else if (const auto ext = path.extension().wstring(); boost::iequals(ext, L".pak"))
        {
        }
        else if (boost::iequals(ext, L".pk3"))
        {
        }
        return nullptr;
    }
    //static
    std::unique_ptr<pack_i> pack_i::create_pack(const std::filesystem::path& path, warning_func_t warn_func)
    {
        if (const auto ext = path.extension().wstring(); boost::iequals(ext, L".pak"))
        {
        }
        else if (boost::iequals(ext, L".pk3"))
        {
        }
        else if (ext.empty())
        {
            auto ppak = std::make_unique<pak_impl::fs_pack_c>();
            ppak->create_pack_impl(path);
            ppak->m_warn_func = warn_func;
            return ppak;
        }
        return nullptr;
    }
}

