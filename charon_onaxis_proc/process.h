#ifndef PROCESS_H
#define PROCESS_H

#include "TCutG.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TTree.h"
#include <vector>

class process
{
public:
    // Constructor/destructor
    process(std::string& name_input, std::string& name_output,
	    int channel);
    ~process();

    // Functions
    void initialize();
    bool check_ifile();
    bool check_ofile_write(bool overwrite_param);
    void time_cut(std::vector<int>& peak_bounds);
    void temp_func();
    void psd_cut(std::vector<int>& peak_bounds, double num_stddevs);
    void write_out(bool overwrite_param);
    
private:
    std::string name_in;
    std::string name_out;
    TFile* f_in;

    TTree* tree;
    ULong64_t num_entries;
    int channel_num;

    // Histograms
    TH2D h_temp;
    TH1D* h_dirty;
    TH1D* h_clean;
    TH2D* h_PSD_dirty;
    TH2D* h_PSD_clean;
    TCutG* pileup_cut;
};

char gather_input();

#endif


    
