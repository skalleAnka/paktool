#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>
#include <iostream>
#include <future>
#include <format>
#include <pack.h>

namespace po = boost::program_options;
namespace fs = std::filesystem;
using namespace std;
using namespace pak;

static void warn_func(const wstring& entry, const wstring& msg)
{
    wcerr << L"  " << entry << L": " << msg << endl; 
}

static fs::path path_strip(const string& str)
{
    return fs::path(boost::trim_right_copy_if(str, [](auto c) { return c == fs::path::preferred_separator; }));
}

static int list_pack(const vector<string>& packs)
{
    for (const auto& name : packs)
    {
        if (auto ppack = pack_i::open_pack(name, pack_i::mode::read_only, &warn_func))
        {
            cout << name << ":" << endl;
            for (const auto& ename : ppack->file_names())
                wcout << " " << ename << endl;
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
        ranges::sort(results, {}, [](const auto& v) { return get<0>(v); });

        for (const auto& [filename, msg] : results)
            wcout << filename << L": " << msg << endl;
    }

    return results.empty() ? 0 : 1;
}

static int convert_pack(const vector<string>& inpack, const string& outpack)
{
    vector<unique_ptr<pack_i>> inpacks;
    ranges::transform(inpack, back_inserter(inpacks),
        [](const auto& v) { return pack_i::open_pack(path_strip(v), pack_i::mode::read_only, &warn_func); });
    
    auto outp = pack_i::open_pack(path_strip(outpack), pack_i::mode::rw_new, warn_func);
    if (outp == nullptr)
    {
        cerr << "Open failed: " << outpack << endl;
        return 1;
    }
    for (auto pinp = begin(inpacks); pinp != end(inpacks); ++pinp)
    {
        const auto& inp = *pinp;  
        for (const auto& filename : inp->file_names()
            | views::transform([](const auto& v) { return wstring{ v }; }))
        {
            if (find_if(pinp + 1, end(inpacks),
                [&](const auto& p) { return p->contains_entry(filename); }) != end(inpacks))
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

static int extract_pack(const vector<string>& inpack, const string& outpack)
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
        if (auto r = convert_pack({ inp }, outp); r != 0)
            return r;
    }
    return 0;
}
 
int main(int argc, char** argv)
{
    po::options_description desc("Usage");
    desc.add_options()
        ("help,h", "Display usage instructions.")
        ("input,i", po::value<vector<string>>(), "One or more input files or folders to process.")
        ("list,l", "List contents of the specified file.")
        ("output,o", po::value<string>(), "Output file (or folder) to convert to (use with -c or -n).")
        ("extract,x", "Extract the contents of the pack file, a new subfolder will be created and named after each pack.")
        ("convert,c", "Convert one or more packs to other formats. Output format determined by file extension.")
        ("compare", "Compare the contents of two packs. Exactly two -i parameters must be given.");

    po::variables_map vm;
    po::store(parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    try
    {
        if (vm.count("help") > 0)
        {
            cout << desc << endl;
        }
        else if (vm.count("list") > 0 && vm.count("input") > 0)
        {
            if (auto r = list_pack(vm["input"].as<vector<string>>()); r != 0)
                return r;
        }
        else if (vm.count("convert") > 0)
        {
            if (vm.count("input") <= 0)
            {
                cerr << "No input files to convert." << endl;
                return 1;
            }

            if (vm.count("output") <= 0)
            {
                cerr << "No output file." << endl;
                return 1;
            }

            if (auto r = convert_pack(vm["input"].as<vector<string>>(), vm["output"].as<string>()); r != 0)
                return r;
        }
        else if (vm.count("extract") > 0)
        {
            const auto outpath = vm.count("output") > 0
                ? vm["output"].as<string>()
                : fs::current_path().string();

            if (vm.count("input") <= 0)
            {
                cerr << "No input files to extract." << endl;
                return 1;
            }

            if (auto r = extract_pack(vm["input"].as<vector<string>>(), outpath); r != 0)
                return r;
        }
        else if (vm.count("compare") > 0)
        {
            if (vm.count("input") > 0)
            {
                const auto cmp = vm["input"].as<vector<string>>();
                if (cmp.size() == 2u)
                    return compare_packs(cmp[0], cmp[1]);

                cerr << "Specify 2 imput files to compare with -i." << endl;
                return 1;
            }
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

