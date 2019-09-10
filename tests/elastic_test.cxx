#include "nlohmann/json.hpp"
#include <cmath>
#include <iostream>

#include "ROOT/RDataFrame.hxx"
#include "ROOT/RVec.hxx"

#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
#include "Math/VectorUtil.h"
#include "TCanvas.h"
#include "TLatex.h"
#include "TStyle.h"
#include "TSystem.h"
R__LOAD_LIBRARY(libMathMore.so)
R__LOAD_LIBRARY(libGenVector.so)

R__LOAD_LIBRARY(libfmt.so)
#include "fmt/core.h"

#include "THStack.h"

#ifdef __cpp_lib_filesystem
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

using RDFNode = decltype(ROOT::RDataFrame{0}.Filter(""));
using Histo1DProxy =
    decltype(ROOT::RDataFrame{0}.Histo1D(ROOT::RDF::TH1DModel{"", "", 128u, 0., 0.}, ""));

struct RDFInfo {
  RDFNode&          df;
  const std::string title;
  RDFInfo(RDFNode& df, std::string_view title) : df{df}, title{title} {}
};

constexpr const double M_P     = .938272;
constexpr const double M_P2    = M_P * M_P;
constexpr const double M_pion  = 0.139;
constexpr const double M_pion2 = M_pion * M_pion;
constexpr const double M_e     = .000511;

// =================================================================================
// Cuts
// =================================================================================
std::string goodTrackSHMS = "P.gtr.dp > -10 && P.gtr.dp < 22";
std::string goodTrackHMS  = "H.gtr.dp > -8 && H.gtr.dp < 8";

std::string piCutSHMS = "P.cal.etottracknorm<1.0";
//std::string piCutSHMS = "P.aero.npeSum > 1.0 && P.cal.eprtracknorm < 0.2 && P.cal.etottracknorm<1.0";

std::string eCutHMS = "H.cal.etottracknorm > 0.50 && H.cal.etottracknorm < 2. && "
                      "H.cer.npeSum > 1.";

std::string epiCut = "P.aero.npeSum > 1.0 && P.cal.eprtracknorm < 0.2 && "
                     "H.cer.npeSum > 1.0 && H.cal.etottracknorm > 0.6 && "
                     "H.cal.etottracknorm < 2.0 && P.cal.etottracknorm<1.0";

using Pvec3D = ROOT::Math::XYZVector;
using Pvec4D = ROOT::Math::PxPyPzMVector;

// =================================================================================
// reconstruction
// =================================================================================
auto p_pion = [](double px, double py, double pz) {
  return Pvec4D{px * 0.996, py * 0.996, pz * 0.996, M_e};
};
auto p_electron = [](double px, double py, double pz) {
  return Pvec4D{px * 0.994, py * 0.994, pz * 0.994, M_e};
};
auto t = [](const double Egamma, Pvec4D& jpsi) {
  Pvec4D beam{0, 0, Egamma, 0};
  return (beam - jpsi).M2();
};

bool root_file_exists(std::string rootfile) {
  bool found_good_file = false;
  if (!gSystem->AccessPathName(rootfile.c_str())) {
    TFile file(rootfile.c_str());
    if (file.IsZombie()) {
      std::cout << rootfile << " is a zombie.\n";
      std::cout
          << " Did your replay finish?  Check that the it is done before running this script.\n";
      return false;
      // return;
    } else {
      std::cout << " using : " << rootfile << "\n";
      return true;
    }
  }
  return false;
}

void elastic_test(int RunNumber = 6012, int nevents = 50000, int prompt = 0, int update = 0,
                        int default_count_goal = 10000, int redo_timing = 0) {

  // ===============================================================================================
  // Initialization
  // ===============================================================================================
  using nlohmann::json;
  json j;
  {
    std::ifstream json_input_file("db2/run_list_coin.json");
    try {
      json_input_file >> j;
    } catch (json::parse_error) {
      std::cerr << "error: json file, db2/run_list.json, is incomplete or has broken syntax.\n";
      std::quick_exit(-127);
    }
  }

  auto runnum_str = std::to_string(RunNumber);
  if (j.find(runnum_str) == j.end()) {
    std::cout << "Run " << RunNumber << " not found in db2/run_list_coin.json\n";
    std::cout << "Check that run number and replay exists. \n";
    std::cout << "If problem persists please contact Sylvester (217-848-0565)\n";
  }
  double P0_shms_setting = j[runnum_str]["spectrometers"]["shms_momentum"].get<double>();
  double P0_shms         = std::abs(P0_shms_setting);

  bool found_good_file = false;

  std::string rootfile =
      fmt::format("full_online/coin_replay_production_{}_{}.root", RunNumber, nevents);
  found_good_file = root_file_exists(rootfile.c_str());
  if (!found_good_file) {
    rootfile =
        fmt::format("ROOTfiles_volatile/coin_replay_production_{}_{}.root", RunNumber, nevents);
    found_good_file = root_file_exists(rootfile.c_str());
  }
  if (!found_good_file) {
    rootfile = fmt::format("ROOTfiles_csv/coin_replay_production_{}_{}.root", RunNumber, nevents);
    found_good_file = root_file_exists(rootfile.c_str());
  }
  if (!found_good_file) {
    rootfile = fmt::format("ROOTfiles/coin_replay_production_{}_{}.root", RunNumber, nevents);
    found_good_file = root_file_exists(rootfile.c_str());
  }
  if (!found_good_file) {
    std::cout << " Error: suitable root file not found\n";
    return;
  }

  // ===============================================================================================
  // Dataframe
  // ===============================================================================================

  ROOT::EnableImplicitMT(24);

  //---------------------------------------------------------------------------
  // Detector tree
  ROOT::RDataFrame d("T", rootfile);

  //// SHMS Scaler tree
  //ROOT::RDataFrame d_sh("TSP", rootfile);
  //// int N_scaler_events = *(d_sh.Count());

  auto d_coin = d.Filter("fEvtHdr.fEvtType == 4");

  // Good track cuts
  auto dHMSGoodTrack  = d_coin.Filter(goodTrackHMS);
  auto dSHMSGoodTrack = d_coin.Filter(goodTrackSHMS);
  auto dCOINGoodTrack = dHMSGoodTrack.Filter(goodTrackSHMS)
                            .Define("p_electron", p_electron, {"H.gtr.px", "H.gtr.py", "H.gtr.pz"})
                            .Define("p_pion", p_pion, {"P.gtr.px", "P.gtr.py", "P.gtr.pz"});
  // PID cuts
  auto dHMSEl  = dHMSGoodTrack.Filter(eCutHMS);
  auto dSHMSEl = dSHMSGoodTrack.Filter(piCutSHMS);
  auto dCOINEl = dCOINGoodTrack.Filter(eCutHMS + " && " + piCutSHMS);
                     //.Filter(
                     //    [=](double npe, double dp) {
                     //      double p_track = P0_shms * (100.0 + dp) / 100.0;
                     //      // no cerenkov cut needed when momentum is below 2.8 GeV/c
                     //      if (p_track < 2.8) {
                     //        return true;
                     //      }
                     //      return npe > 1.0;
                     //    },
                     //    {"P.hgcer.npeSum", "P.gtr.dp"});

  // Timing cuts
  // Find the timing peak
  // Find the coin peak
  double coin_peak_center = 0;
  if (redo_timing) {
    auto h_coin_time =
        dCOINEl.Histo1D({"coin_time", "coin_time", 8000, 0, 1000}, "CTime.ePositronCoinTime_ROC2");
    h_coin_time->DrawClone();
    int coin_peak_bin = h_coin_time->GetMaximumBin();
    coin_peak_center  = h_coin_time->GetBinCenter(coin_peak_bin);
    std::cout << "COINCIDENCE time peak found at: " << coin_peak_center << std::endl;
  } else {
    //coin_peak_center = 43.0; // run 7240-7241
    coin_peak_center = 23.0; // run 6012
    std::cout << "COINCIDENCE time peak: using pre-calculated value at: " << coin_peak_center
              << std::endl;
    ;
  }
  // timing cut lambdas
  // TODO: evaluate timing cut and offset for random background
  auto timing_cut = [=](double coin_time) { return std::abs(coin_time - coin_peak_center) < 2.; };
  // anti-timing set to 5x width of regular
  auto anti_timing_cut = [=](double coin_time) {
    return std::abs(coin_time - coin_peak_center - 28.) < 10.;
  };

  // timing counts
  auto dHMSElInTime  = dHMSEl.Filter(timing_cut, {"CTime.ePositronCoinTime_ROC2"});
  auto dHMSElRandom  = dHMSEl.Filter(anti_timing_cut, {"CTime.ePositronCoinTime_ROC2"});
  auto dSHMSElInTime = dSHMSEl.Filter(timing_cut, {"CTime.ePositronCoinTime_ROC2"});
  auto dSHMSElRandom = dSHMSEl.Filter(anti_timing_cut, {"CTime.ePositronCoinTime_ROC2"});

  auto dCOINElInTime = dCOINEl.Filter(timing_cut, {"CTime.ePiCoinTime_ROC2"});
  auto dCOINElRandom = dCOINEl.Filter(anti_timing_cut, {"CTime.ePiCoinTime_ROC2"});

  // Output root file
  //auto out_file =
  //    new TFile(fmt::format("monitoring/{}/good_csv_counter.root", RunNumber).c_str(), "UPDATE");
  //out_file->cd();

  // =========================================================================================
  // Histograms
  // =========================================================================================
  // 2D correlations
  auto hTheta2DNoCuts = d_coin.Histo2D(
      {"theta2D", "No cuts;#theta_{SHMS};#theta_{HMS};#counts", 50, -.1, .1, 50, -.1, .1},
      "P.gtr.th", "H.gtr.th");
  auto hTheta2DTracking = dCOINGoodTrack.Histo2D(
      {"theta2D", "Cuts: tracking;#theta_{SHMS};#theta_{HMS};#counts", 50, -.1, .1, 50, -.1, .1},
      "P.gtr.th", "H.gtr.th");
  auto hTheta2DPID =
      dCOINEl.Histo2D({"theta2D", "Cuts: tracking+PID;#theta_{SHMS};#theta_{HMS};#counts", 50, -.1,
                       .1, 50, -.1, .1},
                      "P.gtr.th", "H.gtr.th");
  auto hTheta2DTiming =
      dCOINElInTime.Histo2D({"theta2D", "Cuts: tracking+PID;#theta_{SHMS};#theta_{HMS};#counts", 50,
                             -.1, .1, 50, -.1, .1},
                            "P.gtr.th", "H.gtr.th");
  // timing
  auto hCoinTimeNoCuts =
      d_coin.Histo1D({"coin_time.NoCuts", "No Cuts;coin_time;counts", 8000, 0, 1000},
                     "CTime.ePositronCoinTime_ROC2");
  auto hCoinTimeTracking = dCOINGoodTrack.Histo1D(
      {"coin_time.Tracking", "Cuts: Tracking;coin_time;counts", 8000, 0, 1000},
      "CTime.ePositronCoinTime_ROC2");
  auto hCoinTimePID =
      dCOINEl.Histo1D({"coin_time.PID", "Cuts: Tracking+PID;coin_time;counts", 8000, 0, 1000},
                      "CTime.ePositronCoinTime_ROC2");
  auto hCoinTimeTiming = dCOINElInTime.Histo1D(
      {"coin_time.Timing", "Cuts: Tracking+PID+Timing;coin_time;counts", 8000, 0, 1000},
      "CTime.ePositronCoinTime_ROC2");

  auto hRandCoinTimePID = dCOINElRandom.Histo1D(
      {"rand_coin_time.PID", "Cuts: Tracking+PID;coin_time;counts", 8000, 0, 1000},
      "CTime.ePositronCoinTime_ROC2");

  // P.gtr.dp
  auto hPdpNoCuts =
      d_coin.Histo1D({"P.gtr.dp.NoCuts", "No Cuts;#deltap [%];counts", 200, -30, 40}, "P.gtr.dp");
  auto hPdpTracking = dSHMSGoodTrack.Histo1D(
      {"P.gtr.dp.Tracking", "Cuts: Tracking;#deltap [%];counts", 200, -30, 40}, "P.gtr.dp");
  auto hPdpPID = dSHMSEl.Histo1D(
      {"P.gtr.dp.PID", "Cuts: Tracking+PID;#deltap [%];counts", 200, -30, 40}, "P.gtr.dp");
  auto hPdpTiming = dSHMSElInTime.Histo1D(
      {"P.gtr.dp.Timing", "Cuts: Tracking+PID+Timing;#deltap [%];counts", 200, -30, 40},
      "P.gtr.dp");
  // P.gtr.th
  auto hPthNoCuts = d_coin.Histo1D(
      {"P.gtr.th.NoCuts", "No Cuts;#theta_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.th");
  auto hPthTracking = dSHMSGoodTrack.Histo1D(
      {"P.gtr.th.Tracking", "Cuts: Tracking;#theta_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.th");
  auto hPthPID = dSHMSEl.Histo1D(
      {"P.gtr.th.PID", "Cuts: Tracking+PID;#theta_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.th");
  auto hPthTiming = dSHMSElInTime.Histo1D(
      {"P.gtr.th.Timing", "Cuts: Tracking+PID+Timing;#theta_{SHMS};counts", 200, -0.1, 0.1},
      "P.gtr.th");
  // P.gtr.ph
  auto hPphNoCuts =
      d_coin.Histo1D({"P.gtr.ph.NoCuts", "No Cuts;#phi_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.ph");
  auto hPphTracking = dSHMSGoodTrack.Histo1D(
      {"P.gtr.ph.Tracking", "Cuts: Tracking;#phi_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.ph");
  auto hPphPID = dSHMSEl.Histo1D(
      {"P.gtr.ph.PID", "Cuts: Tracking+PID;#phi_{SHMS};counts", 200, -0.1, 0.1}, "P.gtr.ph");
  auto hPphTiming = dSHMSElInTime.Histo1D(
      {"P.gtr.ph.Timing", "Cuts: Tracking+PID+Timing;#phi_{SHMS};counts", 200, -0.1, 0.1},
      "P.gtr.ph");
  // P.gtr.y
  auto hPyNoCuts =
      d_coin.Histo1D({"P.gtr.y.NoCuts", "No Cuts;ytar;counts", 200, -10., 10.}, "P.gtr.y");
  auto hPyTracking = dSHMSGoodTrack.Histo1D(
      {"P.gtr.y.Tracking", "Cuts: Tracking;ytar;counts", 200, -10., 10.}, "P.gtr.y");
  auto hPyPID =
      dSHMSEl.Histo1D({"P.gtr.y.PID", "Cuts: Tracking+PID;ytar;counts", 200, -10., 10.}, "P.gtr.y");
  auto hPyTiming = dSHMSElInTime.Histo1D(
      {"P.gtr.y.Timing", "Cuts: Tracking+PID+Timing;ytar;counts", 200, -10., 10.}, "P.gtr.y");
  // P.cal.etottracknorm
  auto hPcalEPNoCuts =
      d_coin.Histo1D({"P.cal.etottracknorm.NoCuts", "No Cuts;SHMS E/P;counts", 200, -.5, 1.5},
                     "P.cal.etottracknorm");
  auto hPcalEPTracking = dSHMSGoodTrack.Histo1D(
      {"P.cal.etottracknorm.Tracking", "Cuts: Tracking;SHMS E/P;counts", 200, -.5, 1.5},
      "P.cal.etottracknorm");
  auto hPcalEPPID = dSHMSEl.Histo1D(
      {"P.cal.etottracknorm.PID", "Cuts: Tracking+PID;SHMS E/P;counts", 200, -.5, 1.5},
      "P.cal.etottracknorm");
  auto hPcalEPAll = dCOINElInTime.Histo1D(
      {"P.cal.etottracknorm.All", "Cuts: Tracking+PID+Coincidence;SHMS E/P;counts", 200, -.5, 1.5},
      "P.cal.etottracknorm");
  // P.ngcer.npeSum
  auto hPcerNpheNoCuts = d_coin.Histo1D(
      {"P.ngcer.npeSum.NoCuts", "No Cuts;SHMS NGC #phe;counts", 200, -5, 76}, "P.ngcer.npeSum");
  auto hPcerNpheTracking = dSHMSGoodTrack.Histo1D(
      {"P.ngcer.npeSum.Tracking", "Cuts: Tracking;SHMS NGC #phe;counts", 200, -5, 76},
      "P.ngcer.npeSum");
  auto hPcerNphePID = dSHMSEl.Histo1D(
      {"P.ngcer.npeSum.PID", "Cuts: Tracking+PID;SHMS NGC #phe;counts", 200, -5, 76},
      "P.ngcer.npeSum");
  auto hPcerNpheAll = dCOINElInTime.Histo1D(
      {"P.ngcer.npeSum.All", "Cuts: Tracking+PID+Coincidence;SHMS NGC #phe;counts", 200, -5, 76},
      "P.ngcer.npeSum");
  // P.hgcer.npeSum
  auto hPhgcerNpheNoCuts = d_coin.Histo1D(
      {"P.hgcer.npeSum.NoCuts", "No Cuts;SHMS HGC #phe;counts", 200, -5, 76}, "P.hgcer.npeSum");
  auto hPhgcerNpheTracking = dSHMSGoodTrack.Histo1D(
      {"P.hgcer.npeSum.Tracking", "Cuts: Tracking;SHMS HGC #phe;counts", 200, -5, 76},
      "P.hgcer.npeSum");
  auto hPhgcerNphePID = dSHMSEl.Histo1D(
      {"P.hgcer.npeSum.PID", "Cuts: Tracking+PID;SHMS HGC #phe;counts", 200, -5, 76},
      "P.hgcer.npeSum");
  auto hPhgcerNpheAll = dCOINElInTime.Histo1D(
      {"P.hgcer.npeSum.All", "Cuts: Tracking+PID+Coincidence;SHMS HGC #phe;counts", 200, -5, 76},
      "P.hgcer.npeSum");
  // H.cal.etottracknorm
  auto hHcalEPNoCuts =
      d_coin.Histo1D({"H.cal.etottracknorm.NoCuts", "No Cuts;HMS E/P;counts", 200, -.5, 1.5},
                     "H.cal.etottracknorm");
  auto hHcalEPTracking = dHMSGoodTrack.Histo1D(
      {"H.cal.etottracknorm.Tracking", "Cuts: Tracking;HMS E/P;counts", 200, -.5, 1.5},
      "H.cal.etottracknorm");
  auto hHcalEPPID = dHMSEl.Histo1D(
      {"H.cal.etottracknorm.PID", "Cuts: Tracking+PID;HMS E/P;counts", 200, -.5, 1.5},
      "H.cal.etottracknorm");
  auto hHcalEPAll = dCOINElInTime.Histo1D(
      {"H.cal.etottracknorm.All", "Cuts: Tracking+PID+Coincidence;HMS E/P;counts", 200, -.5, 1.5},
      "H.cal.etottracknorm");
  // H.cer.npeSum
  auto hHcerNpheNoCuts = d_coin.Histo1D(
      {"H.cer.npeSum.NoCuts", "No Cuts;HMS Cer #phe;counts", 200, -1, 15}, "H.cer.npeSum");
  auto hHcerNpheTracking = dHMSGoodTrack.Histo1D(
      {"H.cer.npeSum.Tracking", "Cuts: Tracking;HMS Cer #phe;counts", 200, -1, 15}, "H.cer.npeSum");
  auto hHcerNphePID = dHMSEl.Histo1D(
      {"H.cer.npeSum.PID", "Cuts: Tracking+PID+Coincidence;HMS Cer #phe;counts", 200, -1, 15},
      "H.cer.npeSum");
  auto hHcerNpheAll = dCOINElInTime.Histo1D(
      {"H.cer.npeSum.PID", "Cuts: Tracking+PID+Coincidence;HMS Cer #phe;counts", 200, -1, 15},
      "H.cer.npeSum");
  // H.gtr.dp
  auto hHdpNoCuts =
      d_coin.Histo1D({"H.gtr.dp.NoCuts", "No Cuts;#deltap [%];counts", 200, -30, 40}, "H.gtr.dp");
  auto hHdpTracking = dHMSGoodTrack.Histo1D(
      {"H.gtr.dp.Tracking", "Cuts: Tracking;#deltap [%];counts", 200, -30, 40}, "H.gtr.dp");
  auto hHdpPID = dHMSEl.Histo1D(
      {"H.gtr.dp.PID", "Cuts: Tracking+PID;#deltap [%];counts", 200, -30, 40}, "H.gtr.dp");
  auto hHdpTiming = dHMSElInTime.Histo1D(
      {"H.gtr.dp.Timing", "Cuts: Tracking+PID+Timing;#deltap [%];counts", 200, -30, 40},
      "H.gtr.dp");
  // H.gtr.th
  auto hHthNoCuts = d_coin.Histo1D(
      {"H.gtr.th.NoCuts", "No Cuts;#theta_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.th");
  auto hHthTracking = dHMSGoodTrack.Histo1D(
      {"H.gtr.th.Tracking", "Cuts: Tracking;#theta_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.th");
  auto hHthPID = dHMSEl.Histo1D(
      {"H.gtr.th.PID", "Cuts: Tracking+PID;#theta_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.th");
  auto hHthTiming = dHMSElInTime.Histo1D(
      {"H.gtr.th.Timing", "Cuts: Tracking+PID+Timing;#theta_{HMS};counts", 200, -0.1, 0.1},
      "H.gtr.th");
  // H.gtr.ph
  auto hHphNoCuts =
      d_coin.Histo1D({"H.gtr.ph.NoCuts", "No Cuts;#phi_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.ph");
  auto hHphTracking = dHMSGoodTrack.Histo1D(
      {"H.gtr.ph.Tracking", "Cuts: Tracking;#phi_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.ph");
  auto hHphPID = dHMSEl.Histo1D(
      {"H.gtr.ph.PID", "Cuts: Tracking+PID;#phi_{HMS};counts", 200, -0.1, 0.1}, "H.gtr.ph");
  auto hHphTiming = dHMSElInTime.Histo1D(
      {"H.gtr.ph.Timing", "Cuts: Tracking+PID+Timing;#phi_{HMS};counts", 200, -0.1, 0.1},
      "H.gtr.ph");
  // H.gtr.y
  auto hHyNoCuts =
      d_coin.Histo1D({"H.gtr.y.NoCuts", "No Cuts;ytar;counts", 200, -10., 10.}, "H.gtr.y");
  auto hHyTracking = dHMSGoodTrack.Histo1D(
      {"H.gtr.y.Tracking", "Cuts: Tracking;ytar;counts", 200, -10., 10.}, "H.gtr.y");
  auto hHyPID =
      dHMSEl.Histo1D({"H.gtr.y.PID", "Cuts: Tracking+PID;ytar;counts", 200, -10., 10.}, "H.gtr.y");
  auto hHyTiming = dHMSElInTime.Histo1D(
      {"H.gtr.y.Timing", "Cuts: Tracking+PID+Timing;ytar;counts", 200, -10., 10.}, "H.gtr.y");

  // scalers
  //auto total_charge        = d_sh.Max("P.BCM4B.scalerChargeCut");
  //auto shms_el_real_scaler = d_sh.Max("P.pEL_REAL.scaler");
  //auto hms_el_real_scaler  = d_sh.Max("P.hEL_REAL.scaler");
  //auto time_1MHz           = d_sh.Max("P.1MHz.scalerTime");
  //auto time_1MHz_cut       = d_sh.Max("P.1MHz.scalerTimeCut");

  auto yield_all = d.Count();
  // 5 timing cut widths worth of random backgrounds
  auto yield_coin          = d_coin.Count();
  auto yield_HMSGoodTrack  = dHMSGoodTrack.Count();
  auto yield_SHMSGoodTrack = dSHMSGoodTrack.Count();
  auto yield_COINGoodTrack = dCOINGoodTrack.Count();
  auto yield_HMSEl         = dHMSEl.Count();
  auto yield_SHMSEl        = dSHMSEl.Count();
  auto yield_COINEl        = dCOINEl.Count();
  auto yield_HMSElInTime   = dHMSElInTime.Count();
  auto yield_HMSElRandom   = dHMSElRandom.Count();
  auto yield_SHMSElInTime  = dSHMSElInTime.Count();
  auto yield_SHMSElRandom  = dSHMSElRandom.Count();
  auto yield_COINElInTime  = dCOINElInTime.Count();
  auto yield_COINElRandom  = dCOINElRandom.Count();
  auto yield_coin_raw      = dCOINElInTime.Count();
  auto yield_coin_random   = dCOINElRandom.Count();


  // -------------------------------------
  // End lazy eval
  // -------------------------------------
  auto n_coin_good  = *yield_coin_raw - *yield_coin_random / 5.;
  auto n_HMSElGood  = *yield_HMSElInTime - *yield_HMSElRandom / 5;
  auto n_SHMSElGood = *yield_SHMSElInTime - *yield_SHMSElRandom / 5;
  auto n_COINElGood = *yield_COINElInTime - *yield_COINElRandom / 5;

  hPdpNoCuts->DrawCopy();
  //std::cout << "  coin COUNTS : " << *(d_coin.Count()) << "\n";
  //std::cout << " yield_HMSEl : " << *(yield_HMSEl) << "\n";
  std::cout << " yield_COINEl : " << *(yield_COINEl) << "\n";
  //std::cout << "  ALL COUNTS : " << *yield_all << "\n";
  //std::cout << " GOOD COUNTS : " << n_COINElGood << "\n";
  //
  if( 4 != (*(yield_COINEl)) ){
    std::exit(-1);
  }
  std::exit(0);
}
