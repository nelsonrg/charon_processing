#ifndef PROCESS_H
#define PROCESS_H

#include "TCutG.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
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
    void calibrate(double slope, double intercept);
    void psd_cut(double slope, double intercept, double num_stddevs);
    void apply_scaling(std::string& file_name);
    void write_out(bool overwrite_param);
    
private:
    std::string name_in;
    std::string name_out;
    TFile* f_in;

    TTree* tree;
    ULong64_t num_entries;
    int channel_num;
    double scale_factor;

    // Histograms
    TH1D* h_dirty;
    TH1D* h_clean;
    TH2D* h_PSD_dirty;
    TH2D* h_PSD_clean;
    TCutG* pileup_cut;
    TGraph* charge_graph;
};

char gather_input();

#endif


    
