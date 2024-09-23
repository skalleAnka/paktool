 #include <boost/program_options.hpp>
 #include <iostream>
 #include <pack.h>

 namespace po = boost::program_options;
 using namespace std;
 using namespace pak;

 static void warn_func(const wstring& entry, const wstring& msg)
 {
    wcerr << L"  " << entry << L": " << msg << endl; 
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
 
 int main(int argc, char** argv)
 {
    po::options_description desc("Usage");
    desc.add_options()
        ("help,h", "Display usage instructions.")
        ("input,i", po::value<vector<string>>(), "One or more input files or folders to process.")
        ("list,l", "List contents of the specified file.")
        ("output,o", po::value<string>(), "Output file to convert to (use with -c or -n).")
        ("extract,x", "Extract the contents of the pack file, a new subfolder will be created and named after the pack.")
        ("convert,c", "Convert one or more .PAK files to a compressed .PK3 file")
        ("new,n", "Create a new pack from a folder specified with -i. Top level folder will be skipped and pack type is determined from extension.");

    po::variables_map vm;
    po::store(parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    try
    {
        if (vm.count("input") > 0)
        {
            if (auto r = list_pack(vm["input"].as<vector<string>>()); r != 0)
                return r;
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
 }

