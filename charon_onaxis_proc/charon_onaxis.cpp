#include "process.h"
#include "TFile.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <getopt.h>

static void show_usage(std::string name)
{
    std::cerr << "Takes on-axis data from CHARON and processes it into "
	      << "useful histograms.\n\n"
	      << "Usage: " << name << " [OPTION]...\n\n"
              << "Options:\n"
	      << "-i, --input <file>   \t ROOT input file name "
	      <<                            "[default: default_input.root]\n"
	      << "-o, --output <file>  \t ROOT output file name "
	      <<                            "[default: default_output.root]\n"
	      << "-c, --channel <int>  \t digitizer channel "
	      <<                            "[default: 0]\n"
	      << "-s, --stddevs <dble> \t number of standard deviations for "
	      <<                          "pileup cut\n\t\t\t\t[default: 2.0]\n"
	      << "-p, --peakfile <file> \t text file with peak bounds for "
	      <<                             "calibration\n"
	      <<                       "\t\t\t\t[default: default_bounds.txt]\n"
	      << "-w, --overwrite      \t enables overwriting the output file "
	      <<                            "[default: off]\n"
	      << "-h,  --help           \t show this help message\n"
              << std::endl;
};

void read_bounds(std::string& file_name, std::vector<int>& bounds)
{
    std::ifstream f_stream(file_name.c_str());
    int peak_bound;

    if (f_stream) {
	while (f_stream >> peak_bound) {
	    bounds.push_back(peak_bound);
	}
    }
    else {
	std::cerr << "Cannot open peak bounds file, " << file_name << "\n";
    }

    f_stream.close();
};

int main(int argc, char **argv)
{
    // Gather commandline input
    std::cout << "########################################"
	      << "########################################\n";
   
    // Set defaults
    std::string name_input {"default_input.root"};
    std::string name_output {"default_output.root"};
    int channel {0};
    double num_stddevs {2};
    
    std::vector<int> peak_bounds {};
    std::string peak_bound_file {"default_bounds.txt"};
    read_bounds(peak_bound_file, peak_bounds);
    const int num_peaks {3}; // number of peaks to fit
    
    bool overwrite_param {false}; // enforces overwriting output file if it exists
                                  // WARNING. This can be dangerous.
    
    static const struct option long_options[] = {
	{"help", no_argument, 0, 'h'},
	{"input", required_argument, 0, 'i'},
	{"output", required_argument, 0, 'o'},
	{"channel", required_argument, 0, 'c'},
	{"stddevs", required_argument, 0, 's'},
	{"peakfile", required_argument, 0, 'p'},
	{"overwrite", no_argument, 0, 'w'}
    };

    std::string option_string {"i:o:c:s:p:wh"};

    // Parse input
    int opt;
    int option_index {0};
    opt = getopt_long(argc, argv, option_string.c_str(), long_options,
		      &option_index);
    while (opt != -1) {
	switch (opt)
	{
	case 'i':
	    name_input = optarg;
	    break;
	case 'o':
	    name_output = optarg;
	    break;
	case 'c':
	    channel = std::atoi(optarg);
	    break;
	case 's':
	    num_stddevs = std::stod(optarg);
	    break;
	case 'p':
	{
	    std::vector<int> temp {};
	    std::string temp_file {optarg};
	    read_bounds(temp_file, temp);

	    //std::cout << temp.size();

	    if (temp.size() == 2*num_peaks) {
		// load custom file into vector
		peak_bounds.clear();
		peak_bound_file = temp_file;
		read_bounds(peak_bound_file, peak_bounds);
	    }
	    else {
		// keep defaults
		std::cout << "\nWarning! " << temp_file
			  << " was not read properly\n\n";
	    }
	    
	    break;
	}
	case 'w':
	    overwrite_param = true;
	    break;
	case 'h':
	    show_usage(argv[0]);
	    return 1;
	case '?':
	    show_usage(argv[0]);
	    return 1;
	default:
	    break;
	}

	opt = getopt_long(argc, argv, option_string.c_str(), long_options,
			  &option_index);
    }

    if (peak_bounds.size() != 2*num_peaks) {
	std::cout << "\nPeak bound file not loaded properly\n"
		  << "Using built-in defaults\n\n";
	peak_bound_file = "built-in";
	// Manually set bounds to peaks (after no file could be loaded)
	int u_bound_carbon = 17000;
	int l_bound_carbon = 15000;
	peak_bounds.push_back(u_bound_carbon);
	peak_bounds.push_back(l_bound_carbon);

	int u_bound_carbon_esc = 14500;
	int l_bound_carbon_esc = 12000;
	peak_bounds.push_back(u_bound_carbon_esc);
	peak_bounds.push_back(l_bound_carbon_esc);

	int u_bound_2p2mev = 9000;
	int l_bound_2p2mev = 7000;
	peak_bounds.push_back(u_bound_2p2mev);
	peak_bounds.push_back(l_bound_2p2mev);
    }

    // Print out settings for user to see
    std::cout << "\nInput file:\t\t" << name_input << "\n"
	      << "\nOutput file:\t\t" << name_output << "\n"
	      << "\nChannel number:\t\t" << channel << "\n"
	      << "\nStandard deviations:\t" << num_stddevs << "\n"
	      << "\nPeak bound file:\t" << peak_bound_file << "\n"
	      << "\nOverwrite output:\t" << std::boolalpha << overwrite_param << "\n"
	      << std::endl;

    std::cout << "########################################"
	      << "########################################\n";

    // Ask if the user wants to continue with the default parameters
    std::cout << "\nWould you like to continue with these parameters? [y/N]\n";
    char response = gather_input();
    if (response == 'n' || response == 'N') {
	std::cerr << "\nExiting.\n";
	return 1;
    }

    // Perform analysis
    
    // Create object to perform the analysis
    process* P = new process(name_input, name_output, channel);

    // Check if input file exists (for soft failure)
    bool ifile_exists = P->check_ifile();

    if (!ifile_exists) {
	std::cerr << "\nWarning! Input file does not exist!\n"
		  << "Exiting.\n\n";
	return 1;
    }
    
    // Check if the output file exists and if it should be overwritten
    bool ofile_overwrite = P->check_ofile_write(overwrite_param);
    if (ofile_overwrite == false) {
	std::cerr << "\n\nExiting.\n\n";
	return 1;
    }
    
    P->initialize();
    P->time_cut(peak_bounds);
    //P->temp_func();
    P->psd_cut(peak_bounds, num_stddevs);
    P->write_out(overwrite_param);

    return 0;
};
