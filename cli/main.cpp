#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <boost/crc.hpp>
#include <iostream>
#include <future>
#include <format>
#include <numeric>
#include <pack.h>
#include "paktoolver.h"

namespace po = boost::program_options;
namespace fs = std::filesystem;
namespace conv = boost::locale::conv;
using namespace std;
using namespace pak;


template <typename Tfunc>
concept file_filter = std::is_invocable_r_v<bool, Tfunc, wstring_view>;

static void warn_func(const wstring& entry, const wstring& msg)
{
    wcerr << L"  " << entry << L": " << msg << endl; 
}

static fs::path path_strip(const string& str)
{
    return fs::path(boost::trim_right_copy_if(str, [](auto c) { return c == fs::path::preferred_separator; }));
}

static int list_pack(const vector<string>& packs, file_filter auto filter)
{
    for (const auto name : packs
        | views::transform([](const auto& v) { return fs::path(v); }))
    {
        if (auto ppack = pack_i::open_pack(name, pack_i::mode::read_only, &warn_func))
        {
            for (const auto& ename : ppack->file_names() | views::filter(filter))
            {
                if (packs.size() > 1)
                    wcout << name.filename().wstring() << L":";
                wcout << " " << ename << endl;
            }
        }
        else
        {
            cerr << name << ": Could not open." << endl;
            return 1;
        }
    }
    return 0;
}

static auto calc_chksums(const string& pack)
{
    vector<tuple<wstring, uint64_t>> stats;
    
    auto ppack = pack_i::open_pack(pack, pack_i::mode::read_only);
    if (ppack == nullptr)
        throw runtime_error(format("Could not open {}", pack));
    
    stats.reserve(ppack->count());

    ranges::transform(ppack->file_names(), back_inserter(stats), [&](const auto& nm)
    {
        boost::crc_optimal<64, 0x42f0e1eba9ea3693ULL, 0, 0, false, false> crc64;
        if (ppack->open_entry(wstring{ nm }))
        {
            uint8_t buf[0xFFFF];
            for (auto s = ppack->read(buf, size(buf)); s > 0; s = ppack->read(buf, size(buf)))
                crc64.process_bytes(buf, s);
            
            ppack->close_read_entry();
        }
        
        return make_tuple(wstring{ nm }, crc64.checksum());
    });

    ranges::sort(stats, {}, [](const auto& v) { return get<1>(v); });

    return stats;
}

static int compare_packs(const string& pack1, const string& pack2)
{
    auto t2 = async(calc_chksums, pack2);
    const auto st1 = calc_chksums(pack1);
    const auto st2 = t2.get();

    auto packname1 = fs::path(pack1).filename().wstring();
    auto packname2 = fs::path(pack2).filename().wstring();
    if (boost::iequals(packname1, packname2))
    {
        packname1 = L"first";
        packname2 = L"second";
    }

    vector<tuple<wstring, wstring>> results;

    for (const auto& [nm, chk] : st1)
    {
        auto chk2 = st2 | views::values;
        auto nm2 = st2 | views::keys;

        if (auto matches = ranges::subrange(ranges::lower_bound(chk2, chk), ranges::upper_bound(chk2, chk)); !matches.empty())
        {
            if (auto names = ranges::subrange(begin(matches).base(), end(matches).base()) | views::keys;
                ranges::find(names, nm) == end(names))
            {
                if (names.size() > 1u)
                {
                    const vector diffnames(begin(names), end(names));
                    results.emplace_back(nm, format(L"Different names in {}: {}", packname2, boost::join(diffnames, L", ")));
                }
                else
                {
                    results.emplace_back(nm, format(L"Different name in {}: {}", packname2, names.front()));
                }
            }
        }
        else if (ranges::find(nm2, nm) != end(nm2))
        {
            results.emplace_back(nm, L"File is different");
        }
        else
        {
            results.emplace_back(nm, format(L"Only in {}", packname1)); 
        }
    }

    for (const auto& [nm, chk] : st2)
    {
        auto chk1 = st1 | views::values;
        auto nm1 = st1 | views::keys;

        if (auto r = ranges::lower_bound(chk1, chk); r == end(chk1) || *r != chk)
        {
            if (ranges::find(nm1, nm) == end(nm1))
                results.emplace_back(nm, format(L"Only in {}", packname2));
        }
    }

    if (results.empty())
    {
        wcout << L"No differences found." << endl; 
    }
    else
    {
        ranges::sort(results);

        for (const auto& [filename, msg] : results)
            wcout << filename << L": " << msg << endl;
    }

    return results.empty() ? 0 : 1;
}

static int convert_pack(const vector<string>& inpack, const string& outpack, file_filter auto filter)
{
    vector<tuple<unique_ptr<pack_i>, fs::path>> inpacks;
    ranges::transform(inpack, back_inserter(inpacks),
        [](const auto& v)
        {
            const auto p = path_strip(v);
            return make_tuple(pack_i::open_pack(v, pack_i::mode::read_only, &warn_func), p);
        });

    if (auto failed = inpacks | views::filter([](const auto& v) { return get<0>(v) == nullptr; }); !failed.empty())
    {
        for (const auto& failpath : failed | views::values)
            cerr << "Open failed: " << failpath << endl;
        return 1;
    }

    auto pinputs = inpacks | views::keys;
    const auto file_cnt = accumulate(begin(pinputs), end(pinputs), size_t(0),
        [&](auto a, const auto& v) { return a + v->count(filter); });

    if (file_cnt == 0)
        return 0;
    
    auto outp = pack_i::open_pack(path_strip(outpack), pack_i::mode::rw_new, warn_func);
    if (outp == nullptr)
    {
        cerr << "Open failed: " << outpack << endl;
        return 1;
    }

    if (!outp->pre_reserve(file_cnt))
    {
        cerr << "Failed to reserve space in file " << outpack << endl;
        return 1;
    }

    for (auto pinp = begin(pinputs); pinp != end(pinputs); ++pinp)
    {
        const auto& inp = *pinp;  
        for (const auto& filename : inp->file_names() | views::filter(filter)
            | views::transform([](const auto& v) { return wstring{ v }; }))
        {
            if (find_if(pinp + 1, end(pinputs),
                [&](const auto& p) { return p->contains_entry(filename); }) != end(pinputs))
            {
                //File will appear in a later pack so we skip the earlier occurance
                continue;
            }
            wcout << filename << L"...";
            wcout.flush();
            if (inp->open_entry(filename) && outp->new_entry(filename, inp->entry_timestamp()))
            {
                uint8_t buf[0xFFFF];
                for (auto s = inp->read(buf, size(buf)); s > 0; s = inp->read(buf, size(buf)))
                {
                    if (outp->write(buf, s) != s)
                    {
                        cerr << "Write error." << endl;
                        return 1;
                    }
                }
                outp->close_write_entry();
                inp->close_read_entry();
                wcout << L"OK" << endl;
            }
            else
            {
                cerr << "Failed" << endl;
            }
        }
        inp->close_pack();
    }
    outp->close_pack();
    return 0;
}

static int extract_pack(const vector<string>& inpack, const string& outpack, file_filter auto filter)
{
    const auto outdir = fs::path{ outpack };
    if (!fs::is_directory(outdir))
    {
        cerr << outdir.string() << " is not a directory." << endl;
        return 1;
    }

    for (const auto [inp, outp] : inpack
        | views::transform([&](const auto& v)
            { return make_tuple(v, (outpack / fs::path(v).filename().replace_extension(L""))); }))
    {
        if (auto r = convert_pack({ inp }, outp.string(), filter); r != 0)
            return r;
    }
    return 0;
}
 
int main(int argc, char** argv)
{
    po::options_description desc(format("Paktool {}.{}.{} usage", PAKTOOL_MAJOR, PAKTOOL_MINOR, PAKTOOL_PATCH));
    desc.add_options()
        ("help,h", "Display usage instructions.")
        ("list,l", po ::value<vector<string>>()->multitoken(), "List contents of the specified file(s).")
        ("output,o", po::value<string>(), "Output file (or folder) to convert to (use with -c).")
        ("extract,x", po::value<vector<string>>()->multitoken(), "Extract the contents of the pack file, a new subfolder will be created and named after each pack.")
        ("convert,c", po::value<vector<string>>()->multitoken(), "Convert one or more packs to other formats. Output format determined by file extension.")
        ("compare", po::value<vector<string>>()->multitoken(), "Compare the contents of two packs. Exactly two -i parameters must be given.")
        ("filter", po::value<string>(), "Filter for -l, -x, or -c, will match all files that contain the parameter anywhere in the name.");

    try
    {
        po::variables_map vm;
        po::store(parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        auto make_filter = [&]()
        {
            auto enc = boost::locale::util::get_system_locale();
            if (auto r = ranges::find(enc, '.'); r != end(enc))
                enc = { r + 1, end(enc) };

            const auto s = vm.count("filter") > 0
                ? conv::to_utf<wchar_t>(vm["filter"].as<string>(), enc)
                : wstring{};

            return [s](const wstring_view& v) { return s.empty() || boost::icontains(v, s); };
        };

        if (vm.count("help") > 0)
        {
            cout << desc << endl;
        }
        else if (vm.count("list") > 0)
        {
            if (auto r = list_pack(vm["list"].as<vector<string>>(), make_filter()); r != 0)
                return r;
        }
        else if (vm.count("convert") > 0)
        {
            if (vm.count("output") <= 0)
            {
                cerr << "No output file." << endl;
                return 1;
            }

            if (auto r = convert_pack(vm["convert"].as<vector<string>>(), vm["output"].as<string>(), make_filter()); r != 0)
                return r;
        }
        else if (vm.count("extract") > 0)
        {
            const auto outpath = vm.count("output") > 0
                ? vm["output"].as<string>()
                : fs::current_path().string();

            if (auto r = extract_pack(vm["extract"].as<vector<string>>(), outpath, make_filter()); r != 0)
                return r;
        }
        else if (vm.count("compare") > 0)
        {
            const auto cmp = vm["compare"].as<vector<string>>();
            if (cmp.size() == 2u)
                return compare_packs(cmp[0], cmp[1]);

            cerr << "Specify 2 input files to compare with." << endl;
            return 1;
        }
        else
        {
            cerr << "Invalid options." << endl;
            cout << endl << desc << endl;
            return 1;
        }

    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
}

