#include "process.h"
#include "TFile.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

static void show_usage(std::string name)
{
    std::cerr << "Takes on-axis data from CHARON and processes it into "
	      << "useful histograms.\n\n"
	      << "Usage: " << name << " [OPTION]...\n\n"
              << "Options:\n"
              << "-h,  --help           \t show this help message\n"
	      << "--intercept <dble>    \t y-intercept for calibration [default: 0]\n "
	      << "--slope <dble>        \t slope for calibration [default: 1]\n"
              << "-if, --input <file>   \t ROOT input file name "
	      <<                            "[default: default_input.root]\n"
	      << "-of, --output <file>  \t ROOT output file name "
	      <<                            "[default: default_output.root]\n"
	      << "-ch, --channel <int>  \t digitizer channel "
	      <<                            "[default: 0]\n"
	      << "-sd, --stddevs <dble> \t number of standard deviations for "
	      <<                          "pileup cut\n\t\t\t\t[default: 2.0]\n"
	      << "-ow, --overwrite      \t enables overwriting the output file "
	      <<                            "[default: off]\n"
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
	std::cerr << "Cannot open peak bounds file!";
    }

    f_stream.close();
};

int main(int argc, const char* argv[])
{
    // Gather commandline input
    std::cout << "########################################"
	      << "########################################\n";
   
    // Set defaults
    double intercept {0};
    double slope {1};
    std::string name_input {"default_input.root"};
    std::string name_output {"default_output.root"};
    int channel {0};
    double num_stddevs {2};
            
    bool overwrite_param {false}; // enforces overwriting output file if it exists
                                  // WARNING. This can be dangerous.
    

    // Define variables from command line input
    for (int i = 1; i < argc; ++i) {
	std::string option = argv[i];

	if (option == "-h" || option == "--help") {
	    show_usage(argv[0]);
	    return 1;
	}
	else if (option == "--intercept") {
	    intercept = std::stod(argv[i+1]);
	    ++i;
	}
	else if (option == "--slope") {
	    slope = std::stod(argv[i+1]);
	    ++i;
	}
	else if (option == "-if" || option == "--input") {
	    name_input = argv[i+1];
	    ++i;
	}
	else if (option == "-of" || option == "--output") {
	    name_output = argv[i+1];
	    ++i;
	}
	else if (option == "-ch" || option == "--channel") {
	    channel = std::atoi(argv[i+1]);
	    ++i;
	}
	else if (option == "-sd" || option == "--stddevs") {
	    num_stddevs = std::stod(argv[i+1]);
	    ++i;
	}
	else if (option == "-ow" || option == "--overwrite") {
	    overwrite_param = true;
	    ++i;
	}
    }

    // Print out settings for user to see
    std::cout << "\nInput file:\t\t" << name_input << "\n"
	      << "\nOutput file:\t\t" << name_output << "\n"
	      << "\nChannel number:\t\t" << channel << "\n"
	      << "\nIntercept:\t\t" << intercept << "\n"
	      << "\nSlope:\t\t\t" << slope << "\n"
	      << "\nStandard deviations:\t" << num_stddevs << "\n"
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
    P->calibrate(slope, intercept);
    //P->temp_func();
    P->psd_cut(slope, intercept, num_stddevs);
    P->write_out(overwrite_param);

    return 0;
};
