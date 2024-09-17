 #include <boost/program_options.hpp>

 namespace po = boost::program_options;
 using namespace std;
 
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
 }

