#include "process.h"
#include "TCutG.h"
#include "TF1.h"
#include "TLeaf.h"
#include "TLinearFitter.h"
#include "TMath.h"
#include <fstream>
#include <iostream>
#include <vector>
////////////////////////////////////////////////////////////////////////////////
// Non member functions
////////////////////////////////////////////////////////////////////////////////
char gather_input()
{
    // Gets input from user (y,Y,n,N) and returns it as a char
    char check_val;
    std::cin >> check_val;	
    while (check_val != 'n' && check_val != 'N' &&
	   check_val != 'y' && check_val != 'Y') {
	std::cout << "Sorry, I do not understand that response.\n";
	std::cin >> check_val;
    }
    return check_val;
}

////////////////////////////////////////////////////////////////////////////////
// Member functions
////////////////////////////////////////////////////////////////////////////////

// Constructor
process::process(std::string& name_input, std::string& name_output,
		 int channel)
    :
    name_in {name_input}
    ,name_out {name_output}
    ,tree {0}
    ,num_entries {0}
    ,channel_num {channel}
    ,h_dirty {0}
    ,h_clean {0}
    ,h_PSD_dirty {0}
    ,h_PSD_clean {0} 
    ,pileup_cut {0}
{
};

// Destructor
process::~process()
{
    delete f_in;
    delete tree;
    delete h_dirty;
    delete h_clean;
    delete h_PSD_dirty;
    delete h_PSD_clean;
};

// Initial setup (defining some private members)
void process::initialize()
{
    f_in = new TFile(name_in.c_str());
    tree = (TTree*)f_in->Get("WaveformData");
    num_entries = tree->GetEntries();
    std::cout << "\n\nTree read with " <<num_entries<< " events\n\n";

    // Define histogram parameters
    const int num_xbin = 1024;
    const int x_min = 0;  // MeV
    const int x_max = 10; // MeV
    
    // For PSD only (tail/total)
    const int num_ybin = 512;
    const int y_min = 0;
    const int y_max = 1;

    // Create histograms
    h_dirty = new TH1D("Calibrated","Spectrum;Energy [MeV];Counts"
		       ,num_xbin,x_min,x_max);

    h_clean = new TH1D("Pileup_Corrected","Spectrum;Energy [MeV];Counts"
		       ,num_xbin,x_min,x_max);
    
    h_PSD_dirty = new TH2D("Calibrated_PSD","PSD;Energy [MeV];Tail/Total"
			   ,num_xbin,x_min,x_max
			   ,num_ybin,y_min,y_max);

    h_PSD_clean = new TH2D("Clean_PSD","PSD;Energy [MeV];Tail/Total"
			   ,num_xbin,x_min,x_max
			   ,num_ybin,y_min,y_max);
};

// Checks if the designated input file exists
bool process::check_ifile()
{
    // check if input file already exists
    // Returns boolian to main.
    // true --> input file exists
    // false --> input file does not exist
    
    std::ifstream stream(name_in.c_str());
    
    if (stream.good()) 
	return true;
    else
	return false; 
}

// Checks if the designated output file exists already
// Want to be careful not to overwrite something by accident 
bool process::check_ofile_write(bool overwrite_param)
{
    // check if output file already exists
    // Returns boolian to main.
    // true --> overwrite output file
    // false --> stop running the program
    
    char delete_check {'n'};
    char redundant_check {'n'};
    std::ifstream stream(name_out.c_str());
    
    if (stream.good()) {
	// Check if user wants to overwrite file 
	if (overwrite_param == false) {
	    std::cout << "\nThe file '" << name_out.c_str()
		      << "' already exists and you have chosen not to "
		      << "overwrite it.\n";
	    return false;
	}
	std::cout << "\n\nOops!\n\t"
		  << "The output file '" << name_out.c_str()
	          << "' already exists.\n\t"
		  << "Would you like to overwrite it? [y/N]\n\n";
	// Validate input
        delete_check = gather_input();
    }
    else
	return true; // No file exists, so it may be "overwritten"

    // Redundant check (don't want to accidentally overwrite an important file)
    if (delete_check == 'y' || delete_check == 'Y') {
	std::cout << "Are you sure you want to overwrite '"
		  << name_out.c_str() << "'? [y/N]\n";
	// Get input
        redundant_check = gather_input();
	if (redundant_check == 'y' || redundant_check == 'Y')
	    return true;
	else
	    return false;
    }
    else
	return false;
}

// Writes out desired objects to the output file
// Does some cleaning up of objects still in memory (prevents duplicated writes)
void process::write_out(bool overwrite_param)
{
    std::cout << "\n\nWriting output file.\n\n";
    TFile* f_output {0};
    // Need to check if we can overwrite an existing output file
    if (overwrite_param == true)
	f_output = new TFile(name_out.c_str(),"RECREATE");
    else
	f_output = new TFile(name_out.c_str(),"NEW");
    
    // TFile* f_out = &f_output;
    h_dirty->Write();
    h_PSD_dirty->Write();
    h_clean->Write();
    h_PSD_clean->Write();
    pileup_cut->Write();

    delete h_dirty;
    delete h_clean;
    delete h_PSD_dirty;
    delete h_PSD_clean;
    delete pileup_cut;

    f_output->Write();
    f_output->Close();
};

// Creates a calibrated histogram and PSD plot with 60 second timecut windows
// to accound for gain drifting
void process::time_cut(std::vector<int>& peak_bounds)
{
    std::cout << "\n\nProcessing time cuts and calibrating.\n\n";

    // Define histogram parameters
    const int num_xbin = 1024;
    const int adc_min = 0;
    const int adc_max = 35000;
    
    // This histogram temporarily stores uncalibrated data from each timecut
    TH1D* h_temp = new TH1D("temp","Spectrum;Energy [ADC];Counts"
			    ,num_xbin,adc_min,adc_max);

    // Set a 60 second time window
    const int time_sec {60};
    ULong64_t time_window = time_sec/4.0e-9;
    
    // first timestamp
    tree->GetEntry(0);
    ULong64_t time_initial = tree->GetLeaf("TimeStamp")->GetValue(0);
    
    // last timestamp
    tree->GetEntry(num_entries-1);
    ULong64_t time_last = tree->GetLeaf("TimeStamp")->GetValue(0);

    // Initialize variables for the while loop
    ULong64_t time_stamp {0};
    int channel {0};
    double energy {0};
    double tail {0};
    double E_calibrated {0};

    tree->SetBranchAddress("TimeStamp", &time_stamp);
    tree->SetBranchAddress("ChannelID", &channel);
    tree->SetBranchAddress("PSDTotalIntegral", &energy);
    tree->SetBranchAddress("PSDTailIntegral", &tail);
    
    ULong64_t time_start = time_initial; // start of time cut (increments in while-loop)
    ULong64_t time_end = time_start + time_window; // end of time cut (also increments
    ULong64_t entry_start {0};                    //                  in the loop)
    ULong64_t last_entry {0};
    
    while (time_start < time_last) {

	std::cout << static_cast<double>(time_start) / double(time_last)*100
		  << "% Complete\n";
	
	// Fill a temporary histogram for the time cut calibration
	for (ULong64_t entry=entry_start; entry<num_entries; ++entry) {
	    tree->GetEntry(entry);
	   
	    if (time_stamp > time_end) {
		last_entry = entry;
		break;
	    }
	   	   
	    if (channel == channel_num) {
		h_temp->Fill(energy);
	    }
	}

	// Fit peaks
	// 4.44 Photo peak
	int u_bound_0 = peak_bounds.at(0);
	int l_bound_0 = peak_bounds.at(1);
	int bin_0 {0};
	double peak_0 {0};

	h_temp->GetXaxis()->SetRangeUser(l_bound_0,u_bound_0);
	bin_0 = h_temp->GetMaximumBin();
	h_temp->GetXaxis()->SetRange(bin_0-5,bin_0+5);
	peak_0 = h_temp->GetMean();

	// Fit 4.44 Single Escape Peak
	int u_bound_1 = peak_bounds.at(2);
	int l_bound_1 = peak_bounds.at(3);
	int bin_1 {0};
	double peak_1 {0};
    
	h_temp->GetXaxis()->SetRangeUser(l_bound_1,u_bound_1);
	bin_1 = h_temp->GetMaximumBin();
	h_temp->GetXaxis()->SetRange(bin_1-5,bin_1+5);
	peak_1 = h_temp->GetMean();

	// Fit 2.2 MeV Photo Peak
	int u_bound_2 = peak_bounds.at(4);
	int l_bound_2 = peak_bounds.at(5);
	int bin_2 {0};
	double peak_2 {0};
    
	h_temp->GetXaxis()->SetRangeUser(l_bound_2,u_bound_2);
	bin_2 = h_temp->GetMaximumBin();
	h_temp->GetXaxis()->SetRange(bin_2-5,bin_2+5);
	peak_2 = h_temp->GetMean();
	


        // Calibrate w/ linear fit
	TLinearFitter fit1 {TLinearFitter(1,"pol1")};
	double ax[] = {peak_0,peak_1,peak_2};
	double ay[] = {4.438,3.927,2.2};
	fit1.AssignData(3,1,ax,ay);
	fit1.Eval();
	double intercept = fit1.GetParameter(0);
	double slope = fit1.GetParameter(1);
		
	// Calibrate 
	for (ULong64_t entry=entry_start; entry<last_entry; ++entry)
	{
	    tree->GetEntry(entry);
	       	
	    E_calibrated = slope*energy + intercept;
      
	    if (channel == channel_num) {
		if (pileup_cut == 0) {
		    h_dirty->Fill(E_calibrated);
		    h_PSD_dirty->Fill(E_calibrated,tail/energy);
		}
		else if (pileup_cut != 0 &&
		    pileup_cut->IsInside(E_calibrated,tail/energy)) {
		    h_clean->Fill(E_calibrated);
		    h_PSD_clean->Fill(E_calibrated,tail/energy);
		}
	    }
	}

	// Reset temp histogram to reuse
	h_temp->Reset();

	// Increment time boundaries
	entry_start = last_entry;
	time_start += time_window;
	time_end += time_window;
	// End of while loop
    }

    delete h_temp;
};

// Temporary function to load histograms from another file
// Useful for debugging or adding functionality
void process::temp_func()
{
    TFile* f_temp = new TFile("testing_input.root");
    h_temp = *(TH2D*)f_temp->Get("Calibrated_PSD");
    h_PSD_dirty = &h_temp;
    delete f_temp;
    std::cout << "\n\nPSD Plot Loaded\n\n";
}

// Cleans up pileup for a correction later
void process::psd_cut(std::vector<int>& peak_bounds, double num_stddevs = 2)
{
    std::cout << "\n\nProcessing Pileup Cut.\n\nProgress:\n\t";

    std::vector<double> y_bottom {0};
    std::vector<double> y_top {0};
    std::vector<double> x {0};
    
    // Initial guess for the gaussian fit
    // Indexes:
    //        0 --> height
    //        1 --> center
    //        2 --> standard deviation
    //        3 --> offset
    double last_par [] {0,0,0.1,0};

    for (int xbin=0; xbin<=h_PSD_dirty->GetNbinsX(); ++xbin) {
	if (xbin % 100 == 0) {
	    std::cout << static_cast<double>(xbin)/h_PSD_dirty->GetNbinsX()*100
		      << "%\n\t";
	}

	/** Uncomment for a coarser fit  
	if (xbin % 5 != 0) {
	    continue;
	}
	**/

	TH1D* proj_p = h_PSD_dirty->ProjectionY("current_proj", xbin, xbin);
	TH1D proj = *proj_p;

	// Create gaussian fit
	TF1* gaus_fit = new TF1("fit","gaus(0)+[3]",0,1);

	// Set peak parameters
	double peak_bin = proj.GetBinCenter(proj.GetMaximumBin());
	double height {h_PSD_dirty->GetBinContent(peak_bin)};
	double std_dev = last_par[2];
	double offset = last_par[3];

	// Perform fit 
	double par [] {height,peak_bin,std_dev,offset};
	gaus_fit->SetParameters(par);
	proj.Fit(gaus_fit,"Q");
	gaus_fit->GetParameters(par);

	// Check fit
	int n_entries = proj.Integral(1,proj.GetNbinsX());
	if (n_entries < 500)
	    continue; // This number might need to be changed 

	if (par[2] < 0.005)  
	    par[2] = 0.005;

	// Collect coordinates for the TCutG points
	double cut = par[2]*num_stddevs;
	double energy = h_PSD_dirty->GetXaxis()->GetBinCenter(xbin);
	y_bottom.push_back(par[1]-cut);
	y_top.push_back(par[1]+cut);
	x.push_back(energy);

	// Set default fit parameters for the next run
	for (int i=0; i<4; ++i) {
	    last_par[i] = par[i];
	}
	
	// Clean up our object
        delete proj_p;
    }

    // Create TCutG
    const int n = 2*x.size()+1;
    std::cout << "Number of points: " << n << "\n\n";
    pileup_cut = new TCutG("cut",n);

    // Apply moving average to top
    std::vector<double> y_top_new {};
    const int window {5};
    for (std::size_t i=0; i<y_top.size(); ++i) {
    double sum {0};
	double count {0};
	int start_index = i-window;
	if (start_index < 0) {
	    y_top_new.push_back(y_top.at(i));
	    continue;
	    //start_index = 0;
	}
	for (std::size_t j=start_index; j<=i; ++j) {
	    sum += y_top.at(j);
	    ++count;
	}
	y_top_new.push_back(sum / count);
    }

    std::cout << "\n Averaging bottom points\n\n";
    // Apply moving average to bottom
    std::vector<double> y_bottom_new{};
    for (std::size_t i=0; i<y_bottom.size(); ++i) {
	double sum {0};
	double count {0};
	int start_index = i-window;
	if (start_index < 0) {
	    y_bottom_new.push_back(y_bottom.at(i));
	    continue;
	    //start_index = 0;
	}
	for (std::size_t j=start_index; j<=i; ++j) {
	    sum += y_bottom.at(j);
	    ++count;
	}
	y_bottom_new.push_back(sum / count);
    }

    // Fill the cut with points
    for (std::size_t i=0; i<x.size(); ++i) {
	// Start with the top points
	pileup_cut->SetPoint(i,x.at(i),y_top_new.at(i));
    }
    for (std::size_t i=0; i<x.size(); ++i) {
	// Now get the bottom points
	// Read from x and y backwards
	pileup_cut->SetPoint(i+x.size(),x.at(x.size()-1-i)
			     ,y_bottom_new.at(x.size()-1-i));
    }
    pileup_cut->SetPoint(n-1,x.at(0),y_top_new.at(0));

    // Now apply the PSD cut
    std::cout << "\n\nApplying PSD Cut for Pileup Correction.\n\n";

    double num_clean
	,num_total
	,fraction
	,scale_factor;
    
    // Get number of clean events (inside the pileup correction)
    num_clean = pileup_cut->IntegralHist(h_PSD_dirty);
    
    // Get number of total events
    num_total = h_dirty->Integral(1,h_dirty->GetNbinsX());

    fraction = num_clean / num_total;

    // Pileup correction factor
    scale_factor = ( (1 - TMath::Log(fraction)) / fraction);

    // Create histograms
    time_cut(peak_bounds);
    
    // Apply correction factor
    h_clean->Sumw2();
    h_clean->Scale(scale_factor);
}

