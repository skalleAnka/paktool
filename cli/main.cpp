 #include <boost/program_options.hpp>

 namespace po = boost::program_options;
 
 int main(int argc, char** argv)
 {
    po::options_description desc("Usage");
    desc.add_options()
        ("help,h", "Display usage instructions.");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);
 }

