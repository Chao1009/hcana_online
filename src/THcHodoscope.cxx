/** \class THcHodoscope
    \ingroup Detectors

\brief Generic hodoscope consisting of multiple
planes with multiple paddles with phototubes on both ends.

This differs from Hall A scintillator class in that it is the whole
hodoscope array, not just one plane.

*/

#include "TClass.h"
#include "THaSubDetector.h"
#include "THcCherenkov.h"
#include "THcHallCSpectrometer.h"
#include "THcHitList.h"
#include "THcRawShowerHit.h"
#include "THcScintPlaneCluster.h"
#include "THcShower.h"
#include "THcSignalHit.h"
#include "math.h"

#include "TClonesArray.h"
#include "THaCutList.h"
#include "THaDetMap.h"
#include "THaEvData.h"
#include "THaGlobals.h"
#include "THaTrack.h"
#include "THcDetectorMap.h"
#include "THcGlobals.h"
#include "THcHodoscope.h"
#include "THcParmList.h"
#include "TMath.h"
#include "VarDef.h"
#include "VarType.h"

#include "THaOutput.h"
#include "TTree.h"

#include "THaTrackProj.h"
#include <vector>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "hcana/helpers.hxx"
#include <cassert>

using namespace std;
using std::vector;

//_____________________________________________________________________________
THcHodoscope::THcHodoscope(const char* name, const char* description, THaApparatus* apparatus)
    : hcana::ConfigLogging<THaNonTrackingDetector>(name, description, apparatus) {
  // Constructor

  // fTrackProj = new TClonesArray( "THaTrackProj", 5 );
  // Construct the planes
  fNPlanes       = 0; // No planes until we make them
  fStartTime     = -1e5;
  fGoodStartTime = kFALSE;
}

//_____________________________________________________________________________
THcHodoscope::THcHodoscope() : hcana::ConfigLogging<THaNonTrackingDetector>() {
  // Constructor
}

//_____________________________________________________________________________
void THcHodoscope::Setup(const char* name, const char* description) {
  /**
     Create the scintillator plane objects for the hodoscope.

     Uses the Xhodo_num_planes and Xhodo_plane_names to get the number of
     planes and their names.

     Gets a pointer to the Cherenkov named "cer" ("hgcer" in the case of the SHMS.)

  */
  if (IsZombie())
    return;

  // fDebug = 1;  // Keep this at one while we're working on the code

  char prefix[2];

  prefix[0] = tolower(GetApparatus()->GetName()[0]);
  prefix[1] = '\0';

  TString temp(prefix[0]);
  fSHMS = kFALSE;
  if (temp == "p")
    fSHMS = kTRUE;
  TString histname = temp + "_timehist";
  hTime            = new TH1F(histname, "", 400, 0, 200);
  // cout << " fSHMS = " << fSHMS << endl;
  string    planenamelist;
  DBRequest listextra[] = {{"hodo_num_planes", &fNPlanes, kInt},
                           {"hodo_plane_names", &planenamelist, kString},
                           {"hodo_tdcrefcut", &fTDC_RefTimeCut, kInt, 0, 1},
                           {"hodo_adcrefcut", &fADC_RefTimeCut, kInt, 0, 1},
                           {0}};
  // fNPlanes = 4; 		// Default if not defined
  fTDC_RefTimeCut = 0; // Minimum allowed reference times
  fADC_RefTimeCut = 0;
  gHcParms->LoadParmValues((DBRequest*)&listextra, prefix);

  _det_logger->info("Plane Name List : {}", planenamelist);
  // cout << "Plane Name List : " << planenamelist << endl;

  vector<string> plane_names = vsplit(planenamelist);
  // Plane names
  if (plane_names.size() != (UInt_t)fNPlanes) {
    cout << "ERROR: Number of planes " << fNPlanes << " doesn't agree with number of plane names "
         << plane_names.size() << endl;
    // Should quit.  Is there an official way to quit?
  }
  fPlaneNames = new char*[fNPlanes];
  for (Int_t i = 0; i < fNPlanes; i++) {
    fPlaneNames[i] = new char[plane_names[i].length() + 1];
    strcpy(fPlaneNames[i], plane_names[i].c_str());
  }

  // Probably shouldn't assume that description is defined
  char* desc = new char[strlen(description) + 100];
  fPlanes    = new THcScintillatorPlane*[fNPlanes];
  for (Int_t i = 0; i < fNPlanes; i++) {
    strcpy(desc, description);
    strcat(desc, " Plane ");
    strcat(desc, fPlaneNames[i]);
    fPlanes[i] = new THcScintillatorPlane(fPlaneNames[i], desc, i + 1,
                                          this); // Number planes starting from zero!!
    // cout << "Created Scintillator Plane " << fPlaneNames[i] << ", " << desc << endl;
  }

  // Save the nominal particle mass
  THcHallCSpectrometer* app = dynamic_cast<THcHallCSpectrometer*>(GetApparatus());
  fPartMass                 = app->GetParticleMass();
  fBetaNominal              = app->GetBetaAtPcentral();

  if (fSHMS) {
    fCherenkov = dynamic_cast<THcCherenkov*>(app->GetDetector("hgcer"));
  } else {
    fCherenkov = dynamic_cast<THcCherenkov*>(app->GetDetector("cer"));
  }

  delete[] desc;
}

//_____________________________________________________________________________
THaAnalysisObject::EStatus THcHodoscope::Init(const TDatime& date) {
  // cout << "In THcHodoscope::Init()" << endl;
  Setup(GetName(), GetTitle());

  char EngineDID[] = "xSCIN";
  EngineDID[0]     = toupper(GetApparatus()->GetName()[0]);
  if (gHcDetectorMap->FillMap(fDetMap, EngineDID) < 0) {
    static const char* const here = "Init()";
    // Error( Here(here), "Error filling detectormap for %s.", EngineDID );
    _det_logger->error("THcHodoscope::Init : Error filling detectormap for {}.", EngineDID);
    return kInitError;
  }

  // Should probably put this in ReadDatabase as we will know the
  // maximum number of hits after setting up the detector map
  // But it needs to happen before the sub detectors are initialized
  // so that they can get the pointer to the hitlist.
  _det_logger->info("Hodo TDC and ADC ref time cut = {} {}", fTDC_RefTimeCut, fADC_RefTimeCut);
  // cout << " Hodo tdc ref time cut = " << fTDC_RefTimeCut << " " << fADC_RefTimeCut << endl;

  InitHitList(fDetMap, "THcRawHodoHit", fDetMap->GetTotNumChan() + 1, fTDC_RefTimeCut,
              fADC_RefTimeCut);

  EStatus status;
  // This triggers call of ReadDatabase and DefineVariables
  if ((status = THaNonTrackingDetector::Init(date)))
    return fStatus = status;

  for (Int_t ip = 0; ip < fNPlanes; ip++) {
    if ((status = fPlanes[ip]->Init(date))) {
      return fStatus = status;
    }
  }

  // Why not std::vector?
  fNScinHits     = new Int_t[fNPlanes];
  fGoodPlaneTime = new Bool_t[fNPlanes];
  fNPlaneTime    = new Int_t[fNPlanes];
  fSumPlaneTime  = new Double_t[fNPlanes];

  //  Double_t  fHitCnt4 = 0., fHitCnt3 = 0.;

  // Int_t m = 0;
  // fScinHit = new Double_t*[fNPlanes];
  // for ( m = 0; m < fNPlanes; m++ ){
  //   fScinHit[m] = new Double_t[fNPaddle[0]];
  // }

  for (int ip = 0; ip < fNPlanes; ++ip) {
    fScinHitPaddle.push_back(std::vector<Int_t>(fNPaddle[ip], 0));
  }

  fPresentP        = 0;
  THaVar* vpresent = gHaVars->Find(Form("%s.present", GetApparatus()->GetName()));
  if (vpresent) {
    fPresentP = (Bool_t*)vpresent->GetValuePointer();
  }

  return fStatus = kOK;
}
//_____________________________________________________________________________
Int_t THcHodoscope::ReadDatabase(const TDatime& date) {
  /**
     Read this detector's parameters from the ThcParmlist.

     This function is called by THaDetectorBase::Init() once at the
     beginning of the analysis.
  */
  //  static const char* const here = "ReadDatabase()";
  char prefix[2];
  char parname[100];

  // Determine which spectrometer in order to construct
  // the parameter names (e.g. hscin_1x_nr vs. sscin_1x_nr)

  prefix[0] = tolower(GetApparatus()->GetName()[0]);
  //
  prefix[1] = '\0';
  strcpy(parname, prefix);
  strcat(parname, "scin_");
  //  Int_t plen=strlen(parname);
  // cout << " readdatabse hodo fnplanes = " << fNPlanes << endl;

  CreateMissReportParms(Form("%sscin", prefix));

  fBetaNoTrk      = 0.;
  fBetaNoTrkChiSq = 0.;

  fNPaddle      = new UInt_t[fNPlanes];
  fFPTime       = new Double_t[fNPlanes];
  fPlaneCenter  = new Double_t[fNPlanes];
  fPlaneSpacing = new Double_t[fNPlanes];

  prefix[0] = tolower(GetApparatus()->GetName()[0]);
  //
  prefix[1] = '\0';

  for (Int_t i = 0; i < fNPlanes; i++) {

    DBRequest list[] = {{Form("scin_%s_nr", fPlaneNames[i]), &fNPaddle[i], kInt}, {0}};

    gHcParms->LoadParmValues((DBRequest*)&list, prefix);
  }

  // GN added
  // reading variables from *hodo.param
  fMaxScinPerPlane = fNPaddle[0];
  for (Int_t i = 1; i < fNPlanes; i++) {
    fMaxScinPerPlane = (fMaxScinPerPlane > fNPaddle[i]) ? fMaxScinPerPlane : fNPaddle[i];
  }
  // need this for "padded arrays" i.e. 4x16 lists of parameters (GN)
  fMaxHodoScin = fMaxScinPerPlane * fNPlanes;
  if (fDebug >= 1)
    cout << "fMaxScinPerPlane = " << fMaxScinPerPlane << " fMaxHodoScin = " << fMaxHodoScin << endl;

  fHodoVelLight        = new Double_t[fMaxHodoScin];
  fHodoPosSigma        = new Double_t[fMaxHodoScin];
  fHodoNegSigma        = new Double_t[fMaxHodoScin];
  fHodoPosMinPh        = new Double_t[fMaxHodoScin];
  fHodoNegMinPh        = new Double_t[fMaxHodoScin];
  fHodoPosPhcCoeff     = new Double_t[fMaxHodoScin];
  fHodoNegPhcCoeff     = new Double_t[fMaxHodoScin];
  fHodoPosTimeOffset   = new Double_t[fMaxHodoScin];
  fHodoNegTimeOffset   = new Double_t[fMaxHodoScin];
  fHodoPosPedLimit     = new Int_t[fMaxHodoScin];
  fHodoNegPedLimit     = new Int_t[fMaxHodoScin];
  fHodoPosInvAdcOffset = new Double_t[fMaxHodoScin];
  fHodoNegInvAdcOffset = new Double_t[fMaxHodoScin];
  fHodoPosInvAdcLinear = new Double_t[fMaxHodoScin];
  fHodoNegInvAdcLinear = new Double_t[fMaxHodoScin];
  fHodoPosInvAdcAdc    = new Double_t[fMaxHodoScin];
  fHodoNegInvAdcAdc    = new Double_t[fMaxHodoScin];

  // New Time-Walk Calibration Parameters
  fHodoVelFit   = new Double_t[fMaxHodoScin];
  fHodoCableFit = new Double_t[fMaxHodoScin];
  fHodo_LCoeff  = new Double_t[fMaxHodoScin];
  fHodoPos_c1   = new Double_t[fMaxHodoScin];
  fHodoNeg_c1   = new Double_t[fMaxHodoScin];
  fHodoPos_c2   = new Double_t[fMaxHodoScin];
  fHodoNeg_c2   = new Double_t[fMaxHodoScin];
  fHodoSigmaPos = new Double_t[fMaxHodoScin];
  fHodoSigmaNeg = new Double_t[fMaxHodoScin];

  fNHodoscopes             = 2;
  fxLoScin                 = new Int_t[fNHodoscopes];
  fxHiScin                 = new Int_t[fNHodoscopes];
  fyLoScin                 = new Int_t[fNHodoscopes];
  fyHiScin                 = new Int_t[fNHodoscopes];
  fHodoSlop                = new Double_t[fNPlanes];
  fTdcOffset               = new Int_t[fNPlanes];
  fAdcTdcOffset            = new Double_t[fNPlanes];
  fHodoPosAdcTimeWindowMin = new Double_t[fMaxHodoScin];
  fHodoPosAdcTimeWindowMax = new Double_t[fMaxHodoScin];
  fHodoNegAdcTimeWindowMin = new Double_t[fMaxHodoScin];
  fHodoNegAdcTimeWindowMax = new Double_t[fMaxHodoScin];

  for (Int_t ip = 0; ip < fNPlanes; ip++) { // Set a large default window
    fTdcOffset[ip]    = 0;
    fAdcTdcOffset[ip] = 0.0;
  }

  DBRequest list[] = {
      {"cosmicflag", &fCosmicFlag, kInt, 0, 1},
      {"NumPlanesBetaCalc", &fNumPlanesBetaCalc, kInt, 0, 1},
      {"start_time_center", &fStartTimeCenter, kDouble},
      {"start_time_slop", &fStartTimeSlop, kDouble},
      {"scin_tdc_to_time", &fScinTdcToTime, kDouble},
      {"scin_tdc_min", &fScinTdcMin, kDouble},
      {"scin_tdc_max", &fScinTdcMax, kDouble},
      {"tof_tolerance", &fTofTolerance, kDouble, 0, 1},
      {"pathlength_central", &fPathLengthCentral, kDouble},
      {"hodo_pos_sigma", &fHodoPosSigma[0], kDouble, fMaxHodoScin, 1},
      {"hodo_neg_sigma", &fHodoNegSigma[0], kDouble, fMaxHodoScin, 1},
      {"hodo_pos_ped_limit", &fHodoPosPedLimit[0], kInt, fMaxHodoScin, 1},
      {"hodo_neg_ped_limit", &fHodoNegPedLimit[0], kInt, fMaxHodoScin, 1},
      {"tofusinginvadc", &fTofUsingInvAdc, kInt, 0, 1},
      {"xloscin", &fxLoScin[0], kInt, (UInt_t)fNHodoscopes},
      {"xhiscin", &fxHiScin[0], kInt, (UInt_t)fNHodoscopes},
      {"yloscin", &fyLoScin[0], kInt, (UInt_t)fNHodoscopes},
      {"yhiscin", &fyHiScin[0], kInt, (UInt_t)fNHodoscopes},
      {"track_eff_test_num_scin_planes", &fTrackEffTestNScinPlanes, kInt},
      {"trackeff_scint_ydiff_max", &trackeff_scint_ydiff_max, kDouble, 0, 1},
      {"trackeff_scint_xdiff_max", &trackeff_scint_xdiff_max, kDouble, 0, 1},
      {"cer_npe", &fNCerNPE, kDouble, 0, 1},
      {"normalized_energy_tot", &fNormETot, kDouble, 0, 1},
      {"hodo_slop", fHodoSlop, kDouble, (UInt_t)fNPlanes},
      {"debugprintscinraw", &fdebugprintscinraw, kInt, 0, 1},
      {"hodo_tdc_offset", fTdcOffset, kInt, (UInt_t)fNPlanes, 1},
      {"hodo_adc_tdc_offset", fAdcTdcOffset, kDouble, (UInt_t)fNPlanes, 1},
      {"hodo_PosAdcTimeWindowMin", fHodoPosAdcTimeWindowMin, kDouble, (UInt_t)fMaxHodoScin, 1},
      {"hodo_PosAdcTimeWindowMax", fHodoPosAdcTimeWindowMax, kDouble, (UInt_t)fMaxHodoScin, 1},
      {"hodo_NegAdcTimeWindowMin", fHodoNegAdcTimeWindowMin, kDouble, (UInt_t)fMaxHodoScin, 1},
      {"hodo_NegAdcTimeWindowMax", fHodoNegAdcTimeWindowMax, kDouble, (UInt_t)fMaxHodoScin, 1},
      {"dumptof", &fDumpTOF, kInt, 0, 1},
      {"TOFCalib_shtrk_lo", &fTOFCalib_shtrk_lo, kDouble, 0, 1},
      {"TOFCalib_shtrk_hi", &fTOFCalib_shtrk_hi, kDouble, 0, 1},
      {"TOFCalib_cer_lo", &fTOFCalib_cer_lo, kDouble, 0, 1},
      {"TOFCalib_beta_lo", &fTOFCalib_beta_lo, kDouble, 0, 1},
      {"TOFCalib_beta_hi", &fTOFCalib_beta_hi, kDouble, 0, 1},
      {"dumptof_filename", &fTOFDumpFile, kString, 0, 1},
      {0}};

  // Defaults if not defined in parameter file

  trackeff_scint_ydiff_max = 20.;
  trackeff_scint_xdiff_max = 20.;
  for (UInt_t ip = 0; ip < fMaxHodoScin; ip++) {
    fHodoPosAdcTimeWindowMin[ip] = -1000.;
    fHodoPosAdcTimeWindowMax[ip] = 1000.;
    fHodoNegAdcTimeWindowMin[ip] = -1000.;
    fHodoNegAdcTimeWindowMax[ip] = 1000.;

    fHodoPosPedLimit[ip] = 0.0;
    fHodoNegPedLimit[ip] = 0.0;
    fHodoPosSigma[ip]    = 0.2;
    fHodoNegSigma[ip]    = 0.2;
  }
  fTOFCalib_shtrk_lo = -kBig;
  fTOFCalib_shtrk_hi = kBig;
  fTOFCalib_cer_lo   = -kBig;
  fTOFCalib_beta_lo  = -kBig;
  fTOFCalib_beta_hi  = kBig;
  fdebugprintscinraw = 0;
  fDumpTOF           = 0;
  fTOFDumpFile       = "";
  fTofUsingInvAdc    = 1;
  fTofTolerance      = 3.0;
  fNCerNPE           = 2.0;
  fNormETot          = 0.7;
  fCosmicFlag        = 0;
  fNumPlanesBetaCalc = 4;
  // Gets added to each reference time corrected raw TDC value
  // to make sure valid range is all positive.

  gHcParms->LoadParmValues((DBRequest*)&list, prefix);
  if (fCosmicFlag == 1)
    cout << "Setup for cosmics in TOF" << endl;
  // cout << " cosmic flag = " << fCosmicFlag << endl;
  if (fDumpTOF) {
    fDumpOut.open(fTOFDumpFile.c_str());
    if (fDumpOut.is_open()) {
      // fDumpOut << "Hodoscope Time of Flight calibration data" << endl;
    } else {
      fDumpTOF = 0;
      cout << "WARNING: Unable to open TOF Dump file " << fTOFDumpFile << endl;
      cout << "Data for TOF calibration not being written." << endl;
    }
  }

  // cout << " x1 lo = " << fxLoScin[0]
  //      << " x2 lo = " << fxLoScin[1]
  //      << " x1 hi = " << fxHiScin[0]
  //      << " x2 hi = " << fxHiScin[1]
  //      << endl;

  // cout << " y1 lo = " << fyLoScin[0]
  //      << " y2 lo = " << fyLoScin[1]
  //      << " y1 hi = " << fyHiScin[0]
  //      << " y2 hi = " << fyHiScin[1]
  //      << endl;

  // cout << "Hdososcope planes hits for trigger = " << fTrackEffTestNScinPlanes
  //      << " normalized energy min = " << fNormETot
  //      << " number of photo electrons = " << fNCerNPE
  //      << endl;

  // Set scin Velocity/Cable to default
  for (UInt_t i = 0; i < fMaxHodoScin; i++) {
    fHodoVelLight[i] = 15.0;
  }

  if (fTofUsingInvAdc) {
    DBRequest list2[] = {
        {"hodo_vel_light", &fHodoVelLight[0], kDouble, fMaxHodoScin, 1},
        {"hodo_pos_invadc_offset", &fHodoPosInvAdcOffset[0], kDouble, fMaxHodoScin},
        {"hodo_neg_invadc_offset", &fHodoNegInvAdcOffset[0], kDouble, fMaxHodoScin},
        {"hodo_pos_invadc_linear", &fHodoPosInvAdcLinear[0], kDouble, fMaxHodoScin},
        {"hodo_neg_invadc_linear", &fHodoNegInvAdcLinear[0], kDouble, fMaxHodoScin},
        {"hodo_pos_invadc_adc", &fHodoPosInvAdcAdc[0], kDouble, fMaxHodoScin},
        {"hodo_neg_invadc_adc", &fHodoNegInvAdcAdc[0], kDouble, fMaxHodoScin},
        {0}};

    gHcParms->LoadParmValues((DBRequest*)&list2, prefix);
  };
  /* if (!fTofUsingInvAdc) {
      DBRequest list3[]={
    {"hodo_vel_light",                   &fHodoVelLight[0],       kDouble,  fMaxHodoScin},
    {"hodo_pos_minph",                   &fHodoPosMinPh[0],       kDouble,  fMaxHodoScin},
    {"hodo_neg_minph",                   &fHodoNegMinPh[0],       kDouble,  fMaxHodoScin},
    {"hodo_pos_phc_coeff",               &fHodoPosPhcCoeff[0],    kDouble,  fMaxHodoScin},
    {"hodo_neg_phc_coeff",               &fHodoNegPhcCoeff[0],    kDouble,  fMaxHodoScin},
    {"hodo_pos_time_offset",             &fHodoPosTimeOffset[0],  kDouble,  fMaxHodoScin},
    {"hodo_neg_time_offset",             &fHodoNegTimeOffset[0],  kDouble,  fMaxHodoScin},
    {0}
    };


    gHcParms->LoadParmValues((DBRequest*)&list3,prefix);
  */
  DBRequest list4[] = {{"hodo_velFit", &fHodoVelFit[0], kDouble, fMaxHodoScin, 1},
                       {"hodo_cableFit", &fHodoCableFit[0], kDouble, fMaxHodoScin, 1},
                       {"hodo_LCoeff", &fHodo_LCoeff[0], kDouble, fMaxHodoScin, 1},
                       {"c1_Pos", &fHodoPos_c1[0], kDouble, fMaxHodoScin, 1},
                       {"c1_Neg", &fHodoNeg_c1[0], kDouble, fMaxHodoScin, 1},
                       {"c2_Pos", &fHodoPos_c2[0], kDouble, fMaxHodoScin, 1},
                       {"c2_Neg", &fHodoNeg_c2[0], kDouble, fMaxHodoScin, 1},
                       {"TDC_threshold", &fTdc_Thrs, kDouble, 0, 1},
                       {"hodo_PosSigma", &fHodoSigmaPos[0], kDouble, fMaxHodoScin, 1},
                       {"hodo_NegSigma", &fHodoSigmaNeg[0], kDouble, fMaxHodoScin, 1},
                       {0}};

  fTdc_Thrs = 1.0;
  // Set Default Values if NOT defined in param file
  for (UInt_t i = 0; i < fMaxHodoScin; i++) {

    // Turn OFF Time-Walk Correction if param file NOT found
    fHodoPos_c1[i] = 0.0;
    fHodoPos_c2[i] = 0.0;
    fHodoNeg_c1[i] = 0.0;
    fHodoNeg_c2[i] = 0.0;
  }
  for (UInt_t i = 0; i < fMaxHodoScin; i++) {
    // Set scin Velocity/Cable to default
    fHodoCableFit[i] = 0.0;
    fHodoVelFit[i]   = 15.0;
    // set time coeff between paddles to default
    fHodo_LCoeff[i] = 0.0;
  }

  gHcParms->LoadParmValues((DBRequest*)&list4, prefix);

  if (fDebug >= 1) {
    cout << "******* Testing Hodoscope Parameter Reading ***\n";
    cout << "StarTimeCenter = " << fStartTimeCenter << endl;
    cout << "StartTimeSlop = " << fStartTimeSlop << endl;
    cout << "ScintTdcToTime = " << fScinTdcToTime << endl;
    cout << "TdcMin = " << fScinTdcMin << " TdcMax = " << fScinTdcMax << endl;
    cout << "TofTolerance = " << fTofTolerance << endl;
    cout << "*** VelLight ***\n";
    for (Int_t i1 = 0; i1 < fNPlanes; i1++) {
      cout << "Plane " << i1 << endl;
      for (UInt_t i2 = 0; i2 < fMaxScinPerPlane; i2++) {
        cout << fHodoVelLight[GetScinIndex(i1, i2)] << " ";
      }
      cout << endl;
    }
    cout << endl << endl;
    // check fHodoPosPhcCoeff
    /*
    cout <<"fHodoPosPhcCoeff = ";
    for (int i1=0;i1<fMaxHodoScin;i1++) {
      cout<<this->GetHodoPosPhcCoeff(i1)<<" ";
    }
    cout<<endl;
    */
  }
  //
  if ((fTofTolerance > 0.5) && (fTofTolerance < 10000.)) {
    // cout << "USING "<<fTofTolerance<<" NSEC WINDOW FOR FP NO_TRACK CALCULATIONS.\n";
    _det_logger->info("THcHodoscope: Using {} nsec window for fp no_track calculations.",
                      fTofTolerance);
  } else {
    fTofTolerance = 3.0;
    // cout << "*** USING DEFAULT 3 NSEC WINDOW FOR FP NO_TRACK CALCULATIONS!! ***\n";
    _det_logger->warn("THcHodoscope: Using default {} nsec window for fp no_track calculations.",
                      fTofTolerance);
  }
  fRatio_xpfp_to_xfp = 0.00;
  TString SHMS       = "p";
  TString HMS        = "h";
  TString test       = prefix[0];
  if (test == SHMS)
    fRatio_xpfp_to_xfp = 0.0018; // SHMS
  if (test == HMS)
    fRatio_xpfp_to_xfp = 0.0011; // HMS
  cout << " fRatio_xpfp_to_xfp= " << fRatio_xpfp_to_xfp << endl;

  fIsInit = true;
  return kOK;
}

//_____________________________________________________________________________
Int_t THcHodoscope::DefineVariables(EMode mode) {
  /**
    Initialize global variables for histograms and Root tree
  */
  // cout << "THcHodoscope::DefineVariables called " << GetName() << endl;
  if (mode == kDefine && fIsSetup)
    return kOK;
  fIsSetup = (mode == kDefine);

  // Register variables in global list

  RVarDef vars[] = {
      // Move these into THcHallCSpectrometer using track fTracks
      {"beta", "Beta including track info", "fBeta"},
      {"betanotrack", "Beta from scintillator hits", "fBetaNoTrk"},
      {"betachisqnotrack", "Chi square of beta from scintillator hits", "fBetaNoTrkChiSq"},
      {"fpHitsTime", "Time at focal plane from all hits", "fFPTimeAll"},
      {"starttime", "Hodoscope Start Time", "fStartTime"},
      {"goodstarttime", "Hodoscope Good Start Time (logical flag)", "fGoodStartTime"},
      {"goodscinhit", "Hit in fid area", "fGoodScinHits"},
      {"TimeHist_Sigma", "", "fTimeHist_Sigma"},
      {"TimeHist_Peak", "", "fTimeHist_Peak"},
      {"TimeHist_Hits", "", "fTimeHist_Hits"},
      {0}};

  return DefineVarsFromList(vars, mode);
  //  return kOK;
}
//_____________________________________________________________________________

Int_t THcHodoscope::ManualInitTree(TTree* t) {
  // The most direct path to the output tree!!!
  std::string app_name    = GetApparatus()->GetName();
  std::string det_name    = GetName();
  std::string branch_name = (app_name + "_" + det_name + "_data");
  if (t) {
    _det_logger->info("THcHodoscope::ManualInitTree : Adding branch, {}, to output tree",
                      branch_name);
    t->Branch(branch_name.c_str(), &_basic_data, 32000, 99);
  }
  return 0;
}
//_____________________________________________________________________________

THcHodoscope::~THcHodoscope() {
  // Destructor. Remove variables from global list.

  delete[] fFPTime;
  delete[] fPlaneCenter;
  delete[] fPlaneSpacing;

  if (fIsSetup)
    RemoveVariables();
  if (fIsInit)
    DeleteArrays();

  for (int i = 0; i < fNPlanes; ++i) {
    delete fPlanes[i];
    delete[] fPlaneNames[i];
  }
  delete[] fPlanes;
  delete[] fPlaneNames;
}

//_____________________________________________________________________________
void THcHodoscope::DeleteArrays() {
  // Delete member arrays. Used by destructor.
  // Int_t k;
  // for( k = 0; k < fNPlanes; k++){
  //   delete [] fScinHit[k];
  // }
  // delete [] fScinHit;

  delete[] fxLoScin;
  fxLoScin = NULL;
  delete[] fxHiScin;
  fxHiScin = NULL;
  delete[] fyLoScin;
  fyLoScin = NULL;
  delete[] fyHiScin;
  fyHiScin = NULL;
  delete[] fHodoSlop;
  fHodoSlop = NULL;

  delete[] fNPaddle;
  fNPaddle = NULL;
  delete[] fHodoVelLight;
  fHodoVelLight = NULL;
  delete[] fHodoPosSigma;
  fHodoPosSigma = NULL;
  delete[] fHodoNegSigma;
  fHodoNegSigma = NULL;
  delete[] fHodoPosMinPh;
  fHodoPosMinPh = NULL;
  delete[] fHodoNegMinPh;
  fHodoNegMinPh = NULL;
  delete[] fHodoPosPhcCoeff;
  fHodoPosPhcCoeff = NULL;
  delete[] fHodoNegPhcCoeff;
  fHodoNegPhcCoeff = NULL;
  delete[] fHodoPosTimeOffset;
  fHodoPosTimeOffset = NULL;
  delete[] fHodoNegTimeOffset;
  fHodoNegTimeOffset = NULL;
  delete[] fHodoPosPedLimit;
  fHodoPosPedLimit = NULL;
  delete[] fHodoNegPedLimit;
  fHodoNegPedLimit = NULL;
  delete[] fHodoPosInvAdcOffset;
  fHodoPosInvAdcOffset = NULL;
  delete[] fHodoNegInvAdcOffset;
  fHodoNegInvAdcOffset = NULL;
  delete[] fHodoPosInvAdcLinear;
  fHodoPosInvAdcLinear = NULL;
  delete[] fHodoNegInvAdcLinear;
  fHodoNegInvAdcLinear = NULL;
  delete[] fHodoPosInvAdcAdc;
  fHodoPosInvAdcAdc = NULL;
  delete[] fHodoNegInvAdcAdc;
  fHodoNegInvAdcAdc = NULL;
  delete[] fGoodPlaneTime;
  fGoodPlaneTime = NULL;
  delete[] fNPlaneTime;
  fNPlaneTime = NULL;
  delete[] fSumPlaneTime;
  fSumPlaneTime = NULL;
  delete[] fNScinHits;
  fNScinHits = NULL;
  delete[] fTdcOffset;
  fTdcOffset = NULL;
  delete[] fAdcTdcOffset;
  fAdcTdcOffset = NULL;
  delete[] fHodoNegAdcTimeWindowMin;
  fHodoNegAdcTimeWindowMin = NULL;
  delete[] fHodoNegAdcTimeWindowMax;
  fHodoNegAdcTimeWindowMax = NULL;
  delete[] fHodoPosAdcTimeWindowMin;
  fHodoPosAdcTimeWindowMin = NULL;
  delete[] fHodoPosAdcTimeWindowMax;
  fHodoPosAdcTimeWindowMax = NULL;

  delete[] fHodoVelFit;
  fHodoVelFit = NULL;
  delete[] fHodoCableFit;
  fHodoCableFit = NULL;
  delete[] fHodo_LCoeff;
  fHodo_LCoeff = NULL;
  delete[] fHodoPos_c1;
  fHodoPos_c1 = NULL;
  delete[] fHodoNeg_c1;
  fHodoNeg_c1 = NULL;
  delete[] fHodoPos_c2;
  fHodoPos_c2 = NULL;
  delete[] fHodoNeg_c2;
  fHodoNeg_c2 = NULL;
  delete[] fHodoSigmaPos;
  fHodoSigmaPos = NULL;
  delete[] fHodoSigmaNeg;
  fHodoSigmaNeg = NULL;
}

//_____________________________________________________________________________
void THcHodoscope::Clear(Option_t* opt) {
  /*! \brief Clears variables
   *
   *  Called by  THcHodoscope::Decode
   *
   */
  fTimeHist_Sigma = kBig;
  fTimeHist_Peak  = kBig;
  fTimeHist_Hits  = kBig;

  fBeta           = 0.0;
  fBetaNoTrk      = 0.0;
  fBetaNoTrkChiSq = 0.0;
  fStartTime      = -1000.;
  fFPTimeAll      = -1000.;
  fGoodStartTime  = kFALSE;
  fGoodScinHits   = 0;

  if (*opt != 'I') {
    for (Int_t ip = 0; ip < fNPlanes; ip++) {
      fPlanes[ip]->Clear();
      fFPTime[ip]       = 0.;
      fPlaneCenter[ip]  = 0.;
      fPlaneSpacing[ip] = 0.;
      for (UInt_t iPaddle = 0; iPaddle < fNPaddle[ip]; ++iPaddle) {
        fScinHitPaddle[ip][iPaddle] = 0;
      }
    }
  }
  fdEdX.clear();
  fNScinHit.clear();
  fNClust.clear();
  fClustSize.clear();
  fClustPos.clear();
  fThreeScin.clear();
  fGoodScinHitsX.clear();
  fGoodFlags.clear();
}

//_____________________________________________________________________________
Int_t THcHodoscope::Decode(const THaEvData& evdata) {
  /*! \brief Decodes raw data and processes raw data into hits for each instance of
   * THcScintillatorPlane
   *
   *  - Reads raw data using THcHitList::DecodeToHitList
   *  - If one wants to subtract pedestals (assumed to be a set of data at beginning of run)
   *    + Must define "Pedestal_event" cut in the cuts definition file
   *    + For each "Pedestal_event" calls THcScintillatorPlane::AccumulatePedestals and returns
   *    + After First event which is not a  "Pedestal_event" calls
   * THcScintillatorPlane::CalculatePedestals
   *  - For each scintillator plane THcScintillatorPlane::ProcessHits
   *  - Calls THcHodoscope::EstimateFocalPlaneTime
   *
   *
   */
  // Get the Hall C style hitlist (fRawHitList) for this event
  Bool_t present = kTRUE; // Suppress reference time warnings
  if (fPresentP) {        // if this spectrometer not part of trigger
    present = *fPresentP;
  }
  fNHits = DecodeToHitList(evdata, !present);

  //
  // GN: print event number so we can cross-check with engine
  // if (evdata.GetEvNum()>1000)
  //   cout <<"\nhcana_event " << evdata.GetEvNum()<<endl;

  fCheckEvent = evdata.GetEvNum();
  fEventType  = evdata.GetEvType();

  if (gHaCuts->Result("Pedestal_event")) {
    Int_t nexthit = 0;
    for (Int_t ip = 0; ip < fNPlanes; ip++) {

      nexthit = fPlanes[ip]->AccumulatePedestals(fRawHitList, nexthit);
    }
    fAnalyzePedestals = 1; // Analyze pedestals first normal events
    return (0);
  }
  if (fAnalyzePedestals) {
    for (Int_t ip = 0; ip < fNPlanes; ip++) {

      fPlanes[ip]->CalculatePedestals();
    }
    fAnalyzePedestals = 0; // Don't analyze pedestals next event
  }

  // Let each plane get its hits
  Int_t nexthit = 0;

  fNfptimes   = 0;
  Int_t thits = 0;
  for (Int_t ip = 0; ip < fNPlanes; ip++) {

    fPlaneCenter[ip]  = fPlanes[ip]->GetPosCenter(0) + fPlanes[ip]->GetPosOffset();
    fPlaneSpacing[ip] = fPlanes[ip]->GetSpacing();

    //    nexthit = fPlanes[ip]->ProcessHits(fRawHitList, nexthit);
    // GN: select only events that have reasonable TDC values to start with
    // as per the Engine h_strip_scin.f
    nexthit = fPlanes[ip]->ProcessHits(fRawHitList, nexthit);
    thits += fPlanes[ip]->GetNScinHits();
  }
  fStartTime = -1000;
  if (thits > 0)
    EstimateFocalPlaneTime();

  if (fdebugprintscinraw == 1) {
    for (UInt_t ihit = 0; ihit < fNRawHits; ihit++) {
      //    THcRawHodoHit* hit = (THcRawHodoHit *) fRawHitList->At(ihit);
      //    cout << ihit << " : " << hit->fPlane << ":" << hit->fCounter << " : "
      //	 << hit->fADC_pos << " " << hit->fADC_neg << " "  <<  hit->fTDC_pos
      //	 << " " <<  hit->fTDC_neg << endl;
    }
    cout << endl;
  }

  return fNHits;
}

//_____________________________________________________________________________
void THcHodoscope::EstimateFocalPlaneTime() {
  /*! \brief Calculates the Drift Chamber start time and fBetaNoTrk (velocity determined without
   * track info)
   *
   *  - Called by  THcHodoscope::Decode
   *  - selects good scintillator paddle hits
   *     + loops through hits in each scintillator plane and fills histogram array, "timehist", with
   * corrected times for positive and negative ends of each paddle
   *     + Determines the peak of "timehist"
   *
   */
  Int_t ihit      = 0;
  Int_t nscinhits = 0; // Total # hits with at least one good tdc
  hTime->Reset();
  //
  for (Int_t ip = 0; ip < fNPlanes; ip++) {
    Int_t nphits = fPlanes[ip]->GetNScinHits();
    nscinhits += nphits;
    TClonesArray* hodoHits = fPlanes[ip]->GetHits();
    for (Int_t i = 0; i < nphits; i++) {
      THcHodoHit* hit = (THcHodoHit*)hodoHits->At(i);
      if (hit->GetHasCorrectedTimes()) {
        Double_t postime = hit->GetPosTOFCorrectedTime();
        Double_t negtime = hit->GetNegTOFCorrectedTime();
        hTime->Fill(postime);
        hTime->Fill(negtime);
      }
    }
  }
  //
  //
  ihit                      = 0;
  Double_t fpTimeSum        = 0.0;
  fNfptimes                 = 0;
  Int_t    Ngood_hits_plane = 0;
  Double_t Plane_fptime_sum = 0.0;
  Bool_t   goodplanetime[fNPlanes];
  Bool_t   twogoodtimes[nscinhits];
  Double_t tmin               = 0.5 * hTime->GetMaximumBin();
  fTimeHist_Peak              = tmin;
  fTimeHist_Sigma             = hTime->GetRMS();
  fTimeHist_Hits              = hTime->Integral();
  _basic_data.fTimeHist_Peak  = fTimeHist_Peak;
  _basic_data.fTimeHist_Sigma = fTimeHist_Sigma;
  _basic_data.fTimeHist_Hits  = fTimeHist_Hits;
  for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
    goodplanetime[ip]      = kFALSE;
    Int_t         nphits   = fPlanes[ip]->GetNScinHits();
    TClonesArray* hodoHits = fPlanes[ip]->GetHits();
    Ngood_hits_plane       = 0;
    Plane_fptime_sum       = 0.0;
    for (Int_t i = 0; i < nphits; i++) {
      THcHodoHit* hit    = (THcHodoHit*)hodoHits->At(i);
      twogoodtimes[ihit] = kFALSE;
      if (hit->GetHasCorrectedTimes()) {
        Double_t postime = hit->GetPosTOFCorrectedTime();
        Double_t negtime = hit->GetNegTOFCorrectedTime();
        if ((postime > (tmin - fTofTolerance)) && (postime < (tmin + fTofTolerance)) &&
            (negtime > (tmin - fTofTolerance)) && (negtime < (tmin + fTofTolerance))) {
          hit->SetTwoGoodTimes(kTRUE);
          twogoodtimes[ihit] = kTRUE;                      // Both tubes fired
          Int_t    index     = hit->GetPaddleNumber() - 1; //
          Double_t fptime;
          if (fCosmicFlag == 1) {
            fptime = hit->GetScinCorrectedTime() +
                     (fPlanes[ip]->GetZpos() + (index % 2) * fPlanes[ip]->GetDzpos()) /
                         (29.979 * fBetaNominal);
          } else {
            fptime = hit->GetScinCorrectedTime() -
                     (fPlanes[ip]->GetZpos() + (index % 2) * fPlanes[ip]->GetDzpos()) /
                         (29.979 * fBetaNominal);
          }
          Ngood_hits_plane++;
          Plane_fptime_sum += fptime;
          fpTimeSum += fptime;
          fNfptimes++;
          goodplanetime[ip] = kTRUE;
        } else {
          hit->SetTwoGoodTimes(kFALSE);
        }
      }
      ihit++;
    }
    if (Ngood_hits_plane)
      fPlanes[ip]->SetFpTime(Plane_fptime_sum / float(Ngood_hits_plane));
    fPlanes[ip]->SetNGoodHits(Ngood_hits_plane);
  }

  if (fNfptimes > 0) {
    fStartTime     = fpTimeSum / fNfptimes;
    fGoodStartTime = kTRUE;
    fFPTimeAll     = fStartTime;
  } else {
    fStartTime     = fStartTimeCenter;
    fGoodStartTime = kFALSE;
    fFPTimeAll     = fStartTime;
  }
  //
  //
  hTime->Reset();
  //
  if ((goodplanetime[0] || goodplanetime[1]) && (goodplanetime[2] || goodplanetime[3])) {

    Double_t sumW  = 0.;
    Double_t sumT  = 0.;
    Double_t sumZ  = 0.;
    Double_t sumZZ = 0.;
    Double_t sumTZ = 0.;
    Int_t    ihhit = 0;

    for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
      Int_t         nphits   = fPlanes[ip]->GetNScinHits();
      TClonesArray* hodoHits = fPlanes[ip]->GetHits();

      for (Int_t i = 0; i < nphits; i++) {
        Int_t index = ((THcHodoHit*)hodoHits->At(i))->GetPaddleNumber() - 1;

        if (twogoodtimes[ihhit]) {

          Double_t sigma = 0.0;
          if (fTofUsingInvAdc) {
            sigma = 0.5 * (TMath::Sqrt(TMath::Power(fHodoPosSigma[GetScinIndex(ip, index)], 2) +
                                       TMath::Power(fHodoNegSigma[GetScinIndex(ip, index)], 2)));
          } else {
            sigma = 0.5 * (TMath::Sqrt(TMath::Power(fHodoSigmaPos[GetScinIndex(ip, index)], 2) +
                                       TMath::Power(fHodoSigmaNeg[GetScinIndex(ip, index)], 2)));
          }

          Double_t scinWeight = 1 / TMath::Power(sigma, 2);
          Double_t zPosition  = fPlanes[ip]->GetZpos() + (index % 2) * fPlanes[ip]->GetDzpos();
          //	  cout << "hit = " << ihhit + 1 << "   zpos = " << zPosition << "   sigma = " <<
          // sigma << endl; cout << "fHodoSigma+ = " << fHodoSigmaPos[GetScinIndex(ip,index)] <<
          // endl;
          sumW += scinWeight;
          sumT += scinWeight * ((THcHodoHit*)hodoHits->At(i))->GetScinCorrectedTime();
          sumZ += scinWeight * zPosition;
          sumZZ += scinWeight * (zPosition * zPosition);
          sumTZ += scinWeight * zPosition * ((THcHodoHit*)hodoHits->At(i))->GetScinCorrectedTime();

        } // condition of good scin time
        ihhit++;
      } // loop over hits of plane
    }   // loop over planes

    Double_t tmp      = sumW * sumZZ - sumZ * sumZ;
    Double_t t0       = (sumT * sumZZ - sumZ * sumTZ) / tmp;
    Double_t tmpDenom = sumW * sumTZ - sumZ * sumT;

    if (TMath::Abs(tmpDenom) > (1 / 10000000000.0)) {

      fBetaNoTrk      = tmp / tmpDenom;
      fBetaNoTrkChiSq = 0.;
      ihhit           = 0;

      for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) { // Loop over planes
        Int_t         nphits   = fPlanes[ip]->GetNScinHits();
        TClonesArray* hodoHits = fPlanes[ip]->GetHits();

        for (Int_t i = 0; i < nphits; i++) {
          Int_t index = ((THcHodoHit*)hodoHits->At(i))->GetPaddleNumber() - 1;

          if (twogoodtimes[ihhit]) {

            Double_t zPosition = fPlanes[ip]->GetZpos() + (index % 2) * fPlanes[ip]->GetDzpos();
            Double_t timeDif   = (((THcHodoHit*)hodoHits->At(i))->GetScinCorrectedTime() - t0);

            Double_t sigma = 0.0;
            if (fTofUsingInvAdc) {
              sigma = 0.5 * (TMath::Sqrt(TMath::Power(fHodoPosSigma[GetScinIndex(ip, index)], 2) +
                                         TMath::Power(fHodoNegSigma[GetScinIndex(ip, index)], 2)));
            } else {
              sigma = 0.5 * (TMath::Sqrt(TMath::Power(fHodoSigmaPos[GetScinIndex(ip, index)], 2) +
                                         TMath::Power(fHodoSigmaNeg[GetScinIndex(ip, index)], 2)));
            }

            fBetaNoTrkChiSq +=
                ((zPosition / fBetaNoTrk - timeDif) * (zPosition / fBetaNoTrk - timeDif)) /
                (sigma * sigma);

          } // condition for good scin time
          ihhit++;
        } // loop over hits of a plane
      }   // loop over planes

      Double_t pathNorm = 1.0;

      fBetaNoTrk = fBetaNoTrk * pathNorm;
      fBetaNoTrk = fBetaNoTrk / 29.979; // velocity / c

    } // condition for fTmpDenom
    else {
      fBetaNoTrk      = 0.;
      fBetaNoTrkChiSq = -2.;
    } // else condition for fTmpDenom
    //
    fGoodEventTOFCalib = kFALSE;
    if ((fNumPlanesBetaCalc == 4) && goodplanetime[0] && goodplanetime[1] && goodplanetime[2] &&
        goodplanetime[3] && fPlanes[0]->GetNGoodHits() == 1 && fPlanes[1]->GetNGoodHits() == 1 &&
        fPlanes[2]->GetNGoodHits() == 1 && fPlanes[3]->GetNGoodHits() == 1)
      fGoodEventTOFCalib = kTRUE;
    if ((fNumPlanesBetaCalc == 3) && goodplanetime[0] && goodplanetime[1] && goodplanetime[2] &&
        fPlanes[0]->GetNGoodHits() == 1 && fPlanes[1]->GetNGoodHits() == 1 &&
        fPlanes[2]->GetNGoodHits() == 1)
      fGoodEventTOFCalib = kTRUE;
    //
    //
  }
}

//_____________________________________________________________________________
Int_t THcHodoscope::ApplyCorrections(void) { return (0); }
//_____________________________________________________________________________
Double_t THcHodoscope::TimeWalkCorrection(const Int_t& paddle, const ESide side) { return (0.0); }

//_____________________________________________________________________________
Int_t THcHodoscope::CoarseProcess(TClonesArray& tracks) {

  Int_t ntracks = tracks.GetLast() + 1; // Number of reconstructed tracks
  // -------------------------------------------------

  //  fDumpOut << " ntrack =  " << ntracks  << endl;

  if (ntracks > 0) {

    // **MAIN LOOP: Loop over all tracks and get corrected time, tof, beta...
    vector<Double_t> nPmtHit(ntracks);
    vector<Double_t> timeAtFP(ntracks);
    fdEdX.reserve(ntracks);
    fGoodFlags.reserve(ntracks);
    for (Int_t itrack = 0; itrack < ntracks; itrack++) { // Line 133
      nPmtHit[itrack]  = 0;
      timeAtFP[itrack] = 0;

      THaTrack* theTrack = dynamic_cast<THaTrack*>(tracks.At(itrack));
      if (!theTrack)
        return -1;

      for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
        fGoodPlaneTime[ip] = kFALSE;
        fNScinHits[ip]     = 0;
        fNPlaneTime[ip]    = 0;
        fSumPlaneTime[ip]  = 0.;
      }
      std::vector<Double_t>               dedx_temp;
      std::vector<std::vector<GoodFlags>> goodflagstmp1;
      goodflagstmp1.reserve(fNumPlanesBetaCalc);
#if __cplusplus >= 201103L
      fdEdX.push_back(std::move(dedx_temp)); // Create array of dedx per hit
      fGoodFlags.push_back(std::move(goodflagstmp1));
#else
      fdEdX.push_back(dedx_temp); // Create array of dedx per hit
      fGoodFlags.push_back(goodflagstmp1);
#endif
      Int_t    nFPTime   = 0;
      Double_t betaChiSq = -3;
      Double_t beta      = 0;
      //      timeAtFP[itrack] = 0.;
      Double_t sumFPTime = 0.; // Line 138
      fNScinHit.push_back(0);

      //! Calculate all corrected hit times and histogram
      //! This uses a copy of code below. Results are save in time_pos,neg
      //! including the z-pos. correction assuming nominal value of betap
      //! Code is currently hard-wired to look for a peak in the
      //! range of 0 to 100 nsec, with a group of times that all
      //! agree withing a time_tolerance of time_tolerance nsec. The normal
      //! peak position appears to be around 35 nsec.
      //! NOTE: if want to find particles with beta different than
      //! reference particle, need to make sure this is big enough
      //! to accomodate difference in TOF for other particles
      //! Default value in case user hasnt defined something reasonable

      // Loop over scintillator planes.
      // In ENGINE, its loop over good scintillator hits.
      hTime->Reset();
      fTOFCalc.clear();  // SAW - Can we
      fTOFPInfo.clear(); // SAW - combine these two?
      Int_t ihhit = 0;   // Hit # overall

      for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {

        std::vector<GoodFlags> goodflagstmp2;
        goodflagstmp2.reserve(fNScinHits[ip]);
#if __cplusplus >= 201103L
        fGoodFlags[itrack].push_back(std::move(goodflagstmp2));
#else
        fGoodFlags[itrack].push_back(goodflagstmp2);
#endif
        fNScinHits[ip]         = fPlanes[ip]->GetNScinHits();
        TClonesArray* hodoHits = fPlanes[ip]->GetHits();

        Double_t zPos  = fPlanes[ip]->GetZpos();
        Double_t dzPos = fPlanes[ip]->GetDzpos();

        // first loop over hits with in a single plane
        for (Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++) {
          // iphit is hit # within a plane
          THcHodoHit* hit = (THcHodoHit*)hodoHits->At(iphit);

          fTOFPInfo.push_back(TOFPInfo());
          // Can remove these as we will initialize in the constructor
          //	  fTOFPInfo[ihhit].time_pos = -99.0;
          //	  fTOFPInfo[ihhit].time_neg = -99.0;
          //	  fTOFPInfo[ihhit].keep_pos = kFALSE;
          //	  fTOFPInfo[ihhit].keep_neg = kFALSE;
          fTOFPInfo[ihhit].scin_pos_time = 0.0;
          fTOFPInfo[ihhit].scin_neg_time = 0.0;
          fTOFPInfo[ihhit].hit           = hit;
          fTOFPInfo[ihhit].planeIndex    = ip;
          fTOFPInfo[ihhit].hitNumInPlane = iphit;
          fTOFPInfo[ihhit].onTrack       = kFALSE;

          Int_t    paddle    = hit->GetPaddleNumber() - 1;
          Double_t zposition = zPos + (paddle % 2) * dzPos;

          Double_t xHitCoord = theTrack->GetX() + theTrack->GetTheta() * (zposition); // Line 183

          Double_t yHitCoord = theTrack->GetY() + theTrack->GetPhi() * (zposition); // Line 184

          Double_t scinTrnsCoord, scinLongCoord;
          if ((ip == 0) || (ip == 2)) { // !x plane. Line 185
            scinTrnsCoord = xHitCoord;
            scinLongCoord = yHitCoord;
          } else if ((ip == 1) || (ip == 3)) { // !y plane. Line 188
            scinTrnsCoord = yHitCoord;
            scinLongCoord = xHitCoord;
          } else {
            return -1;
          } // Line 195

          fTOFPInfo[ihhit].scinTrnsCoord = scinTrnsCoord;
          fTOFPInfo[ihhit].scinLongCoord = scinLongCoord;

          Double_t scinCenter = fPlanes[ip]->GetPosCenter(paddle) + fPlanes[ip]->GetPosOffset();

          // Index to access the 2d arrays of paddle/scintillator properties
          Int_t    fPIndex   = GetScinIndex(ip, paddle);
          Double_t betatrack = theTrack->GetP() / TMath::Sqrt(theTrack->GetP() * theTrack->GetP() +
                                                              fPartMass * fPartMass);

          if (TMath::Abs(scinCenter - scinTrnsCoord) <
              (fPlanes[ip]->GetSize() * 0.5 + fPlanes[ip]->GetHodoSlop())) { // Line 293

            fTOFPInfo[ihhit].onTrack = kTRUE;
            Double_t zcor            = zposition / (29.979 * betatrack) *
                            TMath::Sqrt(1. + theTrack->GetTheta() * theTrack->GetTheta() +
                                        theTrack->GetPhi() * theTrack->GetPhi());
            fTOFPInfo[ihhit].zcor = zcor;
            if (fCosmicFlag) {
              Double_t zcor = -zposition / (29.979 * 1.0) *
                              TMath::Sqrt(1. + theTrack->GetTheta() * theTrack->GetTheta() +
                                          theTrack->GetPhi() * theTrack->GetPhi());
              fTOFPInfo[ihhit].zcor = zcor;
            }
            Double_t tdc_pos = hit->GetPosTDC();
            if (tdc_pos >= fScinTdcMin && tdc_pos <= fScinTdcMax) {
              Double_t adc_pos       = hit->GetPosADC();
              Double_t adcamp_pos    = hit->GetPosADCpeak();
              Double_t pathp         = fPlanes[ip]->GetPosLeft() - scinLongCoord;
              fTOFPInfo[ihhit].pathp = pathp;
              Double_t timep         = tdc_pos * fScinTdcToTime;
              if (fTofUsingInvAdc) {
                timep -= fHodoPosInvAdcOffset[fPIndex] + pathp / fHodoPosInvAdcLinear[fPIndex] +
                         fHodoPosInvAdcAdc[fPIndex] / TMath::Sqrt(TMath::Max(20.0 * .020, adc_pos));
              } else {
                // Double_t tw_corr_pos =
                // fHodoPos_c1[fPIndex]/pow(adcamp_pos/fTdc_Thrs,fHodoPos_c2[fPIndex]) -
                // fHodoPos_c1[fPIndex]/pow(200./fTdc_Thrs, fHodoPos_c2[fPIndex]);
                Double_t tw_corr_pos = 0.;
                pathp                = scinLongCoord;
                if (adcamp_pos > 0)
                  tw_corr_pos = 1. / pow(adcamp_pos / fTdc_Thrs, fHodoPos_c2[fPIndex]) -
                                1. / pow(200. / fTdc_Thrs, fHodoPos_c2[fPIndex]);
                timep += -tw_corr_pos + fHodo_LCoeff[fPIndex] + pathp / fHodoVelFit[fPIndex];
              }
              fTOFPInfo[ihhit].scin_pos_time = timep;
              timep -= zcor;
              fTOFPInfo[ihhit].time_pos = timep;

              hTime->Fill(timep);
            }
            Double_t tdc_neg = hit->GetNegTDC();
            if (tdc_neg >= fScinTdcMin && tdc_neg <= fScinTdcMax) {
              Double_t adc_neg       = hit->GetNegADC();
              Double_t adcamp_neg    = hit->GetNegADCpeak();
              Double_t pathn         = scinLongCoord - fPlanes[ip]->GetPosRight();
              fTOFPInfo[ihhit].pathn = pathn;
              Double_t timen         = tdc_neg * fScinTdcToTime;
              if (fTofUsingInvAdc) {
                timen -= fHodoNegInvAdcOffset[fPIndex] + pathn / fHodoNegInvAdcLinear[fPIndex] +
                         fHodoNegInvAdcAdc[fPIndex] / TMath::Sqrt(TMath::Max(20.0 * .020, adc_neg));
              } else {
                pathn                = scinLongCoord;
                Double_t tw_corr_neg = 0;
                if (adcamp_neg > 0)
                  tw_corr_neg = 1. / pow(adcamp_neg / fTdc_Thrs, fHodoNeg_c2[fPIndex]) -
                                1. / pow(200. / fTdc_Thrs, fHodoNeg_c2[fPIndex]);
                timen += -tw_corr_neg - 2 * fHodoCableFit[fPIndex] + fHodo_LCoeff[fPIndex] -
                         pathn / fHodoVelFit[fPIndex];
              }
              fTOFPInfo[ihhit].scin_neg_time = timen;
              timen -= zcor;
              fTOFPInfo[ihhit].time_neg = timen;
              hTime->Fill(timen);
            }
          } // condition for cenetr on a paddle
          ihhit++;
        } // First loop over hits in a plane <---------

        //-----------------------------------------------------------------------------------------------
        //------------- First large loop over scintillator hits ends here --------------------
        //-----------------------------------------------------------------------------------------------
      }
      Int_t nhits = ihhit;

      if (0.5 * hTime->GetMaximumBin() > 0) {
        Double_t tmin = 0.5 * hTime->GetMaximumBin();

        for (Int_t ih = 0; ih < nhits; ih++) { // loop over all scintillator hits
          if ((fTOFPInfo[ih].time_pos > (tmin - fTofTolerance)) &&
              (fTOFPInfo[ih].time_pos < (tmin + fTofTolerance))) {
            fTOFPInfo[ih].keep_pos = kTRUE;
          }
          if ((fTOFPInfo[ih].time_neg > (tmin - fTofTolerance)) &&
              (fTOFPInfo[ih].time_neg < (tmin + fTofTolerance))) {
            fTOFPInfo[ih].keep_neg = kTRUE;
          }
        }
      }

      //---------------------------------------------------------------------------------------------
      // ---------------------- Second loop over scint. hits in a plane
      // -----------------------------
      //---------------------------------------------------------------------------------------------

      fdEdX[itrack].reserve(nhits);
      fTOFCalc.reserve(nhits);
      for (Int_t ih = 0; ih < nhits; ih++) {
        THcHodoHit* hit   = fTOFPInfo[ih].hit;
        Int_t       iphit = fTOFPInfo[ih].hitNumInPlane;
        Int_t       ip    = fTOFPInfo[ih].planeIndex;
        //         fDumpOut << " looping over hits = " << ih << " plane = " << ip+1 << endl;
        // Flags are used by THcHodoEff
        fGoodFlags[itrack][ip].reserve(nhits);
        fGoodFlags[itrack][ip].push_back(GoodFlags());
        assert(iphit >= 0 && (size_t)iphit < fGoodFlags[itrack][ip].size());
        fGoodFlags[itrack][ip][iphit].onTrack      = kFALSE;
        fGoodFlags[itrack][ip][iphit].goodScinTime = kFALSE;
        fGoodFlags[itrack][ip][iphit].goodTdcNeg   = kFALSE;
        fGoodFlags[itrack][ip][iphit].goodTdcPos   = kFALSE;

        fTOFCalc.push_back(TOFCalc());
        // Do we set back to false for each track, or just once per event?
        assert(ih >= 0 && (size_t)ih < fTOFCalc.size());
        fTOFCalc[ih].good_scin_time = kFALSE;
        // These need a track index too to calculate efficiencies
        fTOFCalc[ih].good_tdc_pos = kFALSE;
        fTOFCalc[ih].good_tdc_neg = kFALSE;
        fTOFCalc[ih].pindex       = ip;

        Int_t paddle              = hit->GetPaddleNumber() - 1;
        fTOFCalc[ih].hit_paddle   = paddle;
        fTOFCalc[ih].good_raw_pad = paddle;

        //	Double_t scinCenter = fPlanes[ip]->GetPosCenter(paddle) +
        // fPlanes[ip]->GetPosOffset(); 	Double_t scinTrnsCoord =
        // fTOFPInfo[ih].scinTrnsCoord; 	Double_t scinLongCoord =
        // fTOFPInfo[ih].scinLongCoord;

        Int_t fPIndex = GetScinIndex(ip, paddle);

        if (fTOFPInfo[ih].onTrack) {
          fGoodFlags[itrack][ip][iphit].onTrack = kTRUE;
          if (fTOFPInfo[ih].keep_pos) { // 301
            fTOFCalc[ih].good_tdc_pos                = kTRUE;
            fGoodFlags[itrack][ip][iphit].goodTdcPos = kTRUE;
          }
          if (fTOFPInfo[ih].keep_neg) { //
            fTOFCalc[ih].good_tdc_neg                = kTRUE;
            fGoodFlags[itrack][ip][iphit].goodTdcNeg = kTRUE;
          }
          // ** Calculate ave time for scin and error.
          if (fTOFCalc[ih].good_tdc_pos) {
            if (fTOFCalc[ih].good_tdc_neg) {
              fTOFCalc[ih].scin_time =
                  (fTOFPInfo[ih].scin_pos_time + fTOFPInfo[ih].scin_neg_time) / 2.;
              fTOFCalc[ih].scin_time_fp = (fTOFPInfo[ih].time_pos + fTOFPInfo[ih].time_neg) / 2.;

              if (fTofUsingInvAdc) {
                fTOFCalc[ih].scin_sigma =
                    TMath::Sqrt(fHodoPosSigma[fPIndex] * fHodoPosSigma[fPIndex] +
                                fHodoNegSigma[fPIndex] * fHodoNegSigma[fPIndex]) /
                    2.;
              } else {
                fTOFCalc[ih].scin_sigma =
                    TMath::Sqrt(fHodoSigmaPos[fPIndex] * fHodoSigmaPos[fPIndex] +
                                fHodoSigmaNeg[fPIndex] * fHodoSigmaNeg[fPIndex]) /
                    2.;
              }

              fTOFCalc[ih].good_scin_time                = kTRUE;
              fGoodFlags[itrack][ip][iphit].goodScinTime = kTRUE;
            } else {
              fTOFCalc[ih].scin_time    = fTOFPInfo[ih].scin_pos_time;
              fTOFCalc[ih].scin_time_fp = fTOFPInfo[ih].time_pos;

              if (fTofUsingInvAdc) {
                fTOFCalc[ih].scin_sigma = fHodoPosSigma[fPIndex];
              } else {
                fTOFCalc[ih].scin_sigma = fHodoSigmaPos[fPIndex];
              }

              fTOFCalc[ih].good_scin_time                = kTRUE;
              fGoodFlags[itrack][ip][iphit].goodScinTime = kTRUE;
            }
          } else {
            if (fTOFCalc[ih].good_tdc_neg) {
              fTOFCalc[ih].scin_time    = fTOFPInfo[ih].scin_neg_time;
              fTOFCalc[ih].scin_time_fp = fTOFPInfo[ih].time_neg;
              if (fTofUsingInvAdc) {
                fTOFCalc[ih].scin_sigma = fHodoNegSigma[fPIndex];
              } else {
                fTOFCalc[ih].scin_sigma = fHodoSigmaNeg[fPIndex];
              }
              fTOFCalc[ih].good_scin_time                = kTRUE;
              fGoodFlags[itrack][ip][iphit].goodScinTime = kTRUE;
            }
          } // In h_tof.f this includes the following if condition for time at focal plane
            // // because it is written in FORTRAN code

          // c     Get time at focal plane
          if (fTOFCalc[ih].good_scin_time) {

            // scin_time_fp doesn't need to be an array
            // Is this any different than the average of time_pos and time_neg?
            //	    Double_t scin_time_fp = ( fTOFPInfo[ih].time_pos +
            //				      fTOFPInfo[ih].time_neg ) / 2.;
            Double_t scin_time_fp = fTOFCalc[ih].scin_time_fp;

            sumFPTime = sumFPTime + scin_time_fp;
            nFPTime++;

            fSumPlaneTime[ip] = fSumPlaneTime[ip] + scin_time_fp;
            fNPlaneTime[ip]++;
            fNScinHit[itrack]++;

            if ((fTOFCalc[ih].good_tdc_pos) && (fTOFCalc[ih].good_tdc_neg)) {
              nPmtHit[itrack] = nPmtHit[itrack] + 2;
            } else {
              nPmtHit[itrack] = nPmtHit[itrack] + 1;
            }

            fdEdX[itrack].push_back(0.0);
            assert(fNScinHit[itrack] > 0 && (size_t)fNScinHit[itrack] < fdEdX[itrack].size() + 1);

            // --------------------------------------------------------------------------------------------
            if (fTOFCalc[ih].good_tdc_pos) {
              if (fTOFCalc[ih].good_tdc_neg) {
                fdEdX[itrack][fNScinHit[itrack] - 1] =
                    TMath::Sqrt(TMath::Max(0., hit->GetPosADC() * hit->GetNegADC()));
              } else {
                fdEdX[itrack][fNScinHit[itrack] - 1] = TMath::Max(0., hit->GetPosADC());
              }
            } else {
              if (fTOFCalc[ih].good_tdc_neg) {
                fdEdX[itrack][fNScinHit[itrack] - 1] = TMath::Max(0., hit->GetNegADC());
              } else {
                fdEdX[itrack][fNScinHit[itrack] - 1] = 0.0;
              }
            }
            // --------------------------------------------------------------------------------------------

          } // time at focal plane condition
        }   // on track condition

        // ** See if there are any good time measurements in the plane.
        if (fTOFCalc[ih].good_scin_time) {
          fGoodPlaneTime[ip] = kTRUE;
          fTOFCalc[ih].dedx  = fdEdX[itrack][fNScinHit[itrack] - 1];
        } else {
          fTOFCalc[ih].dedx = 0.0;
        }

      } // Second loop over hits of a scintillator plane ends here
      theTrack->SetGoodPlane3(fGoodPlaneTime[2] ? 1 : 0);
      if (fNumPlanesBetaCalc == 4)
        theTrack->SetGoodPlane4(fGoodPlaneTime[3] ? 1 : 0);
      //
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------

      // * * Fit beta if there are enough time measurements (one upper, one lower)
      // From h_tof_fit
      if (((fGoodPlaneTime[0]) || (fGoodPlaneTime[1])) &&
          ((fGoodPlaneTime[2]) || (fGoodPlaneTime[3]))) {

        Double_t sumW  = 0.;
        Double_t sumT  = 0.;
        Double_t sumZ  = 0.;
        Double_t sumZZ = 0.;
        Double_t sumTZ = 0.;

        for (Int_t ih = 0; ih < nhits; ih++) {
          Int_t ip = fTOFPInfo[ih].planeIndex;

          if (fTOFCalc[ih].good_scin_time) {

            Double_t scinWeight = 1 / (fTOFCalc[ih].scin_sigma * fTOFCalc[ih].scin_sigma);
            Double_t zPosition =
                (fPlanes[ip]->GetZpos() + (fTOFCalc[ih].hit_paddle % 2) * fPlanes[ip]->GetDzpos());

            sumW += scinWeight;
            sumT += scinWeight * fTOFCalc[ih].scin_time;
            sumZ += scinWeight * zPosition;
            sumZZ += scinWeight * (zPosition * zPosition);
            sumTZ += scinWeight * zPosition * fTOFCalc[ih].scin_time;

          } // condition of good scin time
        }   // loop over hits

        Double_t tmp      = sumW * sumZZ - sumZ * sumZ;
        Double_t t0       = (sumT * sumZZ - sumZ * sumTZ) / tmp;
        Double_t tmpDenom = sumW * sumTZ - sumZ * sumT;

        if (TMath::Abs(tmpDenom) > (1 / 10000000000.0)) {

          beta      = tmp / tmpDenom;
          betaChiSq = 0.;

          for (Int_t ih = 0; ih < nhits; ih++) {
            Int_t ip = fTOFPInfo[ih].planeIndex;

            if (fTOFCalc[ih].good_scin_time) {

              Double_t zPosition = (fPlanes[ip]->GetZpos() +
                                    (fTOFCalc[ih].hit_paddle % 2) * fPlanes[ip]->GetDzpos());
              Double_t timeDif   = (fTOFCalc[ih].scin_time - t0);
              betaChiSq += ((zPosition / beta - timeDif) * (zPosition / beta - timeDif)) /
                           (fTOFCalc[ih].scin_sigma * fTOFCalc[ih].scin_sigma);

            } // condition for good scin time
          }   // loop over hits

          Double_t pathNorm = TMath::Sqrt(1. + theTrack->GetTheta() * theTrack->GetTheta() +
                                          theTrack->GetPhi() * theTrack->GetPhi());
          // Take angle into account
          beta = beta / pathNorm;
          beta = beta / 29.979; // velocity / c

        } // condition for fTmpDenom
        else {
          beta      = 0.;
          betaChiSq = -2.;
        } // else condition for fTmpDenom
      } else {
        beta      = 0.;
        betaChiSq = -1;
      }

      if (nFPTime != 0) {
        timeAtFP[itrack] = (sumFPTime / nFPTime);
      }
      //
      // ---------------------------------------------------------------------------

      Double_t FPTimeSum  = 0.0;
      Int_t    nFPTimeSum = 0;
      for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
        if (fNPlaneTime[ip] != 0) {
          fFPTime[ip] = (fSumPlaneTime[ip] / fNPlaneTime[ip]);
          FPTimeSum += fSumPlaneTime[ip];
          nFPTimeSum += fNPlaneTime[ip];
        } else {
          fFPTime[ip] = 1000. * (ip + 1);
        }
      }
      Double_t fptime = -1000;
      if (nFPTimeSum > 0)
        fptime = FPTimeSum / nFPTimeSum;
      fFPTimeAll    = fptime;
      Double_t dedx = 0.0;
      for (UInt_t ih = 0; ih < fTOFCalc.size(); ih++) {
        if (fTOFCalc[ih].good_scin_time) {
          dedx = fTOFCalc[ih].dedx;
          break;
        }
      }
      theTrack->SetDedx(dedx);
      theTrack->SetFPTime(fptime);
      theTrack->SetBeta(beta);
      theTrack->SetBetaChi2(betaChiSq);
      theTrack->SetNPMT(nPmtHit[itrack]);
      theTrack->SetFPTime(timeAtFP[itrack]);

    } // Main loop over tracks ends here.

  } // If condition for at least one track

  // OriginalTrackEffTest();
  TrackEffTest();

  return 0;
}

void THcHodoscope::TrackEffTest(void) {
  // assume X planes are 0,2 and Y planes are 1,3
  std::array<int, 4> PadLow  = {fxLoScin[0], fyLoScin[0], fxLoScin[1], fyLoScin[1]};
  std::array<int, 4> PadHigh = {fxHiScin[0], fyHiScin[0], fxHiScin[1], fyHiScin[1]};

  std::array<double, 4> PadPosLo;
  std::array<double, 4> PadPosHi;

  for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
    Double_t lowtemp = fPlanes[ip]->GetPosCenter(PadLow[ip] - 1) + fPlanes[ip]->GetPosOffset();
    Double_t hitemp  = fPlanes[ip]->GetPosCenter(PadHigh[ip] - 1) + fPlanes[ip]->GetPosOffset();
    if (lowtemp < hitemp) {
      PadPosLo[ip] = lowtemp;
      PadPosHi[ip] = hitemp;
    } else {
      PadPosLo[ip] = hitemp;
      PadPosHi[ip] = lowtemp;
    }
  }
  //{
  //  std::vector<int>  test_vector = {1,2,5,6,7, 9,10, 20};
  //  auto test_res = hcana::find_discontinuity(test_vector);
  //  std::cout << " find_discontinuity    test: " << *test_res << "\n";
  //  std::cout << " count_discontinuities test: " << hcana::count_discontinuities(test_vector)
  //            << "\n";
  //  auto cont_ranges  = hcana::get_discontinuities(test_vector);
  //  int ir = 0;
  //    std::cout << "ranges: \n";
  //  for(auto r : cont_ranges) {
  //    for(auto v = r.first; v< r.second; v++) {
  //      std::cout << *v << " ";
  //    }
  //    std::cout << "\n";
  //  }
  //}
  //{
  //  std::vector<int>  test_vector = {0,2,4,8,10,12,13, 16,18,20,23};
  //  auto              test_res    = hcana::find_discontinuity(
  //      test_vector, [](const int& x1, const int& x2) { return x1 + 2 == x2; });
  //  std::cout << " find_discontinuity test2: " << *test_res << "\n";
  //  std::cout << " count_discontinuities test2: "
  //            << hcana::count_discontinuities(
  //                   test_vector, [](const int& x1, const int& x2) { return x1 + 2 == x2; })
  //            << "\n";
  //}

  using HitIterator      = std::vector<THcHodoHit*>::iterator;
  using HitRangeVector   = typename std::vector<std::pair<HitIterator, HitIterator>>;
  using ClusterPositions = typename std::vector<double>;
  std::map<int, std::vector<THcHodoHit*>> hit_vecs;
  std::vector<HitRangeVector>             hit_clusters;
  std::vector<ClusterPositions>           pos_clusters;
  std::vector<std::vector<int>>           good_clusters;
  std::array<int, 4>                      n_good_clusters;
  // Loop over each plane
  for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {

    // Get the hit array
    TClonesArray* hodoHits = fPlanes[ip]->GetHits();

    // Create vector for good hits.
    hit_vecs[ip] = std::vector<THcHodoHit*>();
    for (Int_t iphit = 0; iphit < fPlanes[ip]->GetNScinHits(); iphit++) {
      THcHodoHit* hit = (THcHodoHit*)hodoHits->At(iphit);
      // keep only those with "two good times"
      if (hit->GetTwoGoodTimes()) {
        hit_vecs[ip].push_back(hit);
      }
    }

    // Cluster (group adjacent) hits
    if (hit_vecs[ip].size() > 0) {
      auto& hv = hit_vecs[ip];
      hcana::sort(hv, [](const THcHodoHit* h1, const THcHodoHit* h2) {
        return h1->GetPaddleNumber() < h2->GetPaddleNumber();
      });

      // get the clusters for this layer
      hit_clusters.push_back(
          hcana::get_discontinuities(hv, [](const THcHodoHit* h1, const THcHodoHit* h2) {
            return h1->GetPaddleNumber() + 1 == h2->GetPaddleNumber();
          }));
    } else {
      hit_clusters.push_back(HitRangeVector{});
    }
    fPlanes[ip]->SetNumberClusters(hit_clusters.back().size());

    // Calculate each cluster's mean position (in one coordinate)
    ClusterPositions clust_positions;
    std::vector<int> clust_bound;
    int              ic = 0;
    for (auto& r : hit_clusters.back()) {
      double a_pos = 0.0;
      int    n_hit = 0;
      for (auto chit = r.first; chit < r.second; chit++) {
        a_pos +=
            fPlanes[ip]->GetPosCenter((*chit)->GetPaddleNumber() - 1) + fPlanes[ip]->GetPosOffset();
        n_hit++;
      }
      // take the average position
      double avg_pos = a_pos / double(n_hit);
      fPlanes[ip]->SetCluster(ic, avg_pos);
      fPlanes[ip]->SetClusterSize(ic, n_hit);
      // Calculate if it is the bound by the upper and lower limits where we expect
      // full tracks to be reconstructed.
      int is_bound = int((avg_pos >= PadPosLo[ip]) && (avg_pos <= PadPosHi[ip]));
      clust_bound.push_back(is_bound);
      if (is_bound) {
        // we are only interested in clusters positioned inthe "sweet spot"
        clust_positions.push_back(avg_pos);
        n_good_clusters[ip]++;
        if (n_hit > 10) {
          _det_logger->warn("cluster in hodoscope track efficiency has {} hits", n_hit);
        }
      }
      ic += 1;
    }
    pos_clusters.push_back(clust_positions);
    good_clusters.push_back(clust_bound);
    //_logger->info("{} clusters in Hod plane {}", hit_clusters.back().size(), ip );
  }

  bool has_both_x_clusters          = (n_good_clusters[0] > 0) && (n_good_clusters[2] > 0);
  bool has_both_y_clusters          = (n_good_clusters[1] > 0) && (n_good_clusters[3] > 0);
  bool at_least_one_good_x_clusters = (n_good_clusters[0] > 0) || (n_good_clusters[2] > 0);
  bool at_least_one_good_y_clusters = (n_good_clusters[1] > 0) || (n_good_clusters[3] > 0);
  bool good_x_match                 = false;
  bool good_y_match                 = false;

  // Superficial matching. Find out if clusters in front and back could possibly belong to the same
  // track. This done by checking to see if the position differences  are less than 10 cm;
  // ie, |x1-x2|<10  or |y1-y2| <10
  // TODO do actual matching with tracks elsewhere
  if (has_both_x_clusters) {
    int ic0 = 0;

    // factor to project X1 to X2 z position
    const double x1_projection_factor =
        (1 + fRatio_xpfp_to_xfp * (fPlanes[2]->GetZpos() - fPlanes[0]->GetZpos()));

    for (auto x1_pos : pos_clusters[0]) {
      int ic2 = 0;
      // project X1 to X2 Z position
      Double_t x1_proj = x1_pos * x1_projection_factor;
      for (auto x2_pos : pos_clusters[2]) {
        if (std::abs(x1_proj - x2_pos) < trackeff_scint_xdiff_max) {
          good_x_match = true;
          fPlanes[0]->SetClusterUsedFlag(ic0, 1.);
          fPlanes[2]->SetClusterUsedFlag(ic2, 1.);
          break;
        }
        ic2 += 1;
      }
      ic0 += 1;
    }
  }
  if (has_both_y_clusters) {
    int ic1 = 0;
    for (auto y1_pos : pos_clusters[1]) {
      int ic3 = 0;
      for (auto y2_pos : pos_clusters[3]) {
        if (std::abs(y1_pos - y2_pos) < trackeff_scint_ydiff_max) {
          good_y_match = true;
          fPlanes[1]->SetClusterUsedFlag(ic1, 1.);
          fPlanes[3]->SetClusterUsedFlag(ic3, 1.);
          break;
        }
        ic3 += 1;
      }
      ic1 += 1;
    }
  }

  fGoodScinHits = 0;
  // (2+2) 4 good hits
  if (fTrackEffTestNScinPlanes == 4 || fTrackEffTestNScinPlanes == 3) {
    if (good_y_match && good_x_match) {
      fGoodScinHits = 1;
    }
    // (2+1) 3 good hits
    if (at_least_one_good_y_clusters && good_x_match) {
      fGoodScinHits = 1;
    }
    // (1+2) 3 good hits
    if (at_least_one_good_x_clusters && good_y_match) {
      fGoodScinHits = 1;
    }
  }
}
//
//
//
void THcHodoscope::OriginalTrackEffTest(void) {
  /**
      Translation of h_track_tests.f file for tracking efficiency
  */

  //************************now look at some hodoscope tests
  //  *second, we move the scintillators.  here we use scintillator cuts to see
  //  *if a track should have been found.
  cout << " enter track eff" << fNumPlanesBetaCalc << endl;
  for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
    cout << " loop over planes " << ip + 1 << endl;

    TClonesArray* hodoHits = fPlanes[ip]->GetHits();
    //    TClonesArray* scinPosTDC = fPlanes[ip]->GetPosTDC();
    //    TClonesArray* scinNegTDC = fPlanes[ip]->GetNegTDC();

    fNScinHits[ip] = fPlanes[ip]->GetNScinHits();
    cout << " hits =  " << fNScinHits[ip] << endl;
    for (Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++) {
      Int_t paddle = ((THcHodoHit*)hodoHits->At(iphit))->GetPaddleNumber() - 1;

      fScinHitPaddle[ip][paddle] = 1;
      cout << " hit =  " << iphit + 1 << " " << paddle + 1 << endl;
    }
  }

  //  *next, look for clusters of hits in a scin plane.  a cluster is a group of
  //  *adjacent scintillator hits separated by a non-firing scintillator.
  //  *Wwe count the number of three adjacent scintillators too.  (A signle track
  //  *shouldn't fire three adjacent scintillators.

  for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
    // Planes ip = 0 = 1X
    // Planes ip = 2 = 2X
    fNClust.push_back(0);
    fThreeScin.push_back(0);
  }

  // *look for clusters in x planes... (16 scins)  !this assume both x planes have same
  // *number of scintillators.
  cout << " looking for cluster in x planes" << endl;
  Int_t icount;
  for (Int_t ip = 0; ip < 3; ip += 2) {
    icount = 0;
    if (fScinHitPaddle[ip][0] == 1)
      icount++;
    cout << "plane =" << ip << "check if paddle 1 hit " << icount << endl;

    for (Int_t ipaddle = 0; ipaddle < (Int_t)fNPaddle[ip] - 1; ipaddle++) {
      // !look for number of clusters of 1 or more hits
      if ((fScinHitPaddle[ip][ipaddle] == 0) && (fScinHitPaddle[ip][ipaddle + 1] == 1))
        icount++;
      cout << " paddle =  " << ipaddle + 1 << " " << icount << endl;

    } // Loop over  paddles
    cout << "Two  cluster in plane =  " << ip + 1 << " " << icount << endl;
    fNClust[ip] = icount;
    icount      = 0;

    for (Int_t ipaddle = 0; ipaddle < (Int_t)fNPaddle[ip] - 2; ipaddle++) {
      // !look for three or more adjacent hits

      if ((fScinHitPaddle[ip][ipaddle] == 1) && (fScinHitPaddle[ip][ipaddle + 1] == 1) &&
          (fScinHitPaddle[ip][ipaddle + 2] == 1))
        icount++;
    } // Second loop over paddles
    cout << "Three  clusters in plane =  " << ip + 1 << " " << icount << endl;

    if (icount > 0)
      fThreeScin[ip] = 1;

  } // Loop over X plane

  // *look for clusters in y planes... (10 scins)  !this assume both y planes have same
  // *number of scintillators.
  cout << " looking for cluster in y planes" << endl;

  for (Int_t ip = 1; ip < fNumPlanesBetaCalc; ip += 2) {
    // Planes ip = 1 = 1Y
    // Planes ip = 3 = 2Y

    icount = 0;
    if (fScinHitPaddle[ip][0] == 1)
      icount++;
    cout << "plane =" << ip << "check if paddle 1 hit " << icount << endl;

    for (Int_t ipaddle = 0; ipaddle < (Int_t)fNPaddle[ip] - 1; ipaddle++) {
      //  !look for number of clusters of 1 or more hits

      if ((fScinHitPaddle[ip][ipaddle] == 0) && (fScinHitPaddle[ip][ipaddle + 1] == 1))
        icount++;
      cout << " paddle =  " << ipaddle + 1 << " " << icount << endl;

    } // Loop over Y paddles
    cout << "Two  cluster in plane =  " << ip + 1 << " " << icount << endl;

    fNClust[ip] = icount;
    icount      = 0;

    for (Int_t ipaddle = 0; ipaddle < (Int_t)fNPaddle[ip] - 2; ipaddle++) {
      // !look for three or more adjacent hits

      if ((fScinHitPaddle[ip][ipaddle] == 1) && (fScinHitPaddle[ip][ipaddle + 1] == 1) &&
          (fScinHitPaddle[ip][ipaddle + 2] == 1))
        icount++;

    } // Second loop over Y paddles
    cout << "Three  clusters in plane =  " << ip + 1 << " " << icount << endl;

    if (icount > 0)
      fThreeScin[ip] = 1;

  } // Loop over Y planes

  // *next we mask out the edge scintillators, and look at triggers that happened
  // *at the center of the acceptance.  To change which scins are in the mask
  // *change the values of h*loscin and h*hiscin in htracking.param

  //      fGoodScinHits = 0;
  for (Int_t ifidx = fxLoScin[0]; ifidx < (Int_t)fxHiScin[0]; ifidx++) {
    fGoodScinHitsX.push_back(0);
  }

  fHitSweet1X = 0;
  fHitSweet2X = 0;
  fHitSweet1Y = 0;
  fHitSweet2Y = 0;
  // *first x plane.  first see if there are hits inside the scin region
  for (Int_t ifidx = fxLoScin[0] - 1; ifidx < fxHiScin[0]; ifidx++) {
    if (fScinHitPaddle[0][ifidx] == 1) {
      fHitSweet1X  = 1;
      fSweet1XScin = ifidx + 1;
    }
  }

  // *  next make sure nothing fired outside the good region
  for (Int_t ifidx = 0; ifidx < fxLoScin[0] - 1; ifidx++) {
    if (fScinHitPaddle[0][ifidx] == 1) {
      fHitSweet1X = -1;
    }
  }
  for (Int_t ifidx = fxHiScin[0]; ifidx < (Int_t)fNPaddle[0]; ifidx++) {
    if (fScinHitPaddle[0][ifidx] == 1) {
      fHitSweet1X = -1;
    }
  }

  // *second x plane.  first see if there are hits inside the scin region
  for (Int_t ifidx = fxLoScin[1] - 1; ifidx < fxHiScin[1]; ifidx++) {
    if (fScinHitPaddle[2][ifidx] == 1) {
      fHitSweet2X  = 1;
      fSweet2XScin = ifidx + 1;
    }
  }
  // *  next make sure nothing fired outside the good region
  for (Int_t ifidx = 0; ifidx < fxLoScin[1] - 1; ifidx++) {
    if (fScinHitPaddle[2][ifidx] == 1) {
      fHitSweet2X = -1;
    }
  }
  for (Int_t ifidx = fxHiScin[1]; ifidx < (Int_t)fNPaddle[2]; ifidx++) {
    if (fScinHitPaddle[2][ifidx] == 1) {
      fHitSweet2X = -1;
    }
  }

  // *first y plane.  first see if there are hits inside the scin region
  for (Int_t ifidx = fyLoScin[0] - 1; ifidx < fyHiScin[0]; ifidx++) {
    if (fScinHitPaddle[1][ifidx] == 1) {
      fHitSweet1Y  = 1;
      fSweet1YScin = ifidx + 1;
    }
  }
  // *  next make sure nothing fired outside the good region
  for (Int_t ifidx = 0; ifidx < fyLoScin[0] - 1; ifidx++) {
    if (fScinHitPaddle[1][ifidx] == 1) {
      fHitSweet1Y = -1;
    }
  }
  for (Int_t ifidx = fyHiScin[0]; ifidx < (Int_t)fNPaddle[1]; ifidx++) {
    if (fScinHitPaddle[1][ifidx] == 1) {
      fHitSweet1Y = -1;
    }
  }

  // *second y plane.  first see if there are hits inside the scin region
  for (Int_t ifidx = fyLoScin[1] - 1; ifidx < fyHiScin[1]; ifidx++) {
    if (fScinHitPaddle[3][ifidx] == 1) {
      fHitSweet2Y  = 1;
      fSweet2YScin = ifidx + 1;
    }
  }

  // *  next make sure nothing fired outside the good region
  for (Int_t ifidx = 0; ifidx < fyLoScin[1] - 1; ifidx++) {
    if (fScinHitPaddle[3][ifidx] == 1) {
      fHitSweet2Y = -1;
    }
  }
  for (Int_t ifidx = fyHiScin[1]; ifidx < (Int_t)fNPaddle[3]; ifidx++) {
    if (fScinHitPaddle[3][ifidx] == 1) {
      fHitSweet2Y = -1;
    }
  }

  fTestSum = fHitSweet1X + fHitSweet2X + fHitSweet1Y + fHitSweet2Y;

  // * now define a 3/4 or 4/4 trigger of only good scintillators the value
  // * is specified in htracking
  if (fTestSum >= fTrackEffTestNScinPlanes) {
    fGoodScinHits = 1;
    for (Int_t ifidx = fxLoScin[0]; ifidx < fxHiScin[0]; ifidx++) {
      if (fSweet1XScin == ifidx)
        fGoodScinHitsX[ifidx] = 1;
    }
  }

  // * require front/back hodoscopes be close to each other
  if ((fGoodScinHits == 1) && (fTrackEffTestNScinPlanes == 4)) {
    if (TMath::Abs(fSweet1XScin - fSweet2XScin) > 3)
      fGoodScinHits = 0;
    if (TMath::Abs(fSweet1YScin - fSweet2YScin) > 2)
      fGoodScinHits = 0;
  }
  //
}
//_____________________________________________________________________________
Int_t THcHodoscope::FineProcess(TClonesArray& tracks) {
  Int_t    Ntracks = tracks.GetLast() + 1; // Number of reconstructed tracks
  Double_t hitPos;
  Double_t hitDistance;
  Int_t    ih = 0;
  for (Int_t itrk = 0; itrk < Ntracks; itrk++) {
    THaTrack* theTrack = static_cast<THaTrack*>(tracks[itrk]);
    if (theTrack->GetIndex() == 0) {
      fBeta                       = theTrack->GetBeta();
      fFPTimeAll                  = theTrack->GetFPTime();
      _basic_data.fBeta           = fBeta;
      _basic_data.fFPTimeAll      = fFPTimeAll;
      Double_t shower_track_enorm = theTrack->GetEnergy() / theTrack->GetP();
      for (Int_t ip = 0; ip < fNumPlanesBetaCalc; ip++) {
        Double_t      pl_xypos     = 0;
        Double_t      pl_zpos      = 0;
        Int_t         num_good_pad = 0;
        TClonesArray* hodoHits     = fPlanes[ip]->GetHits();
        for (Int_t iphit = 0; iphit < fPlanes[ip]->GetNScinHits(); iphit++) {
          THcHodoHit* hit = fTOFPInfo[ih].hit;
          if (fTOFCalc[ih].good_tdc_pos && fTOFCalc[ih].good_tdc_neg) {
            Bool_t sh_pid   = (shower_track_enorm > fTOFCalib_shtrk_lo &&
                             shower_track_enorm < fTOFCalib_shtrk_hi);
            Bool_t beta_pid = (fBeta > fTOFCalib_beta_lo && fBeta < fTOFCalib_beta_hi);
            // cer_pid is true if there is no Cherenkov detector
            Bool_t cer_pid = (fCherenkov ? (fCherenkov->GetCerNPE() > fTOFCalib_cer_lo) : kTRUE);
            if (fDumpTOF && Ntracks == 1 && fGoodEventTOFCalib && sh_pid && beta_pid && cer_pid) {
              fDumpOut << fixed << setprecision(2);
              fDumpOut << showpoint << " 1" << setw(3) << ip + 1 << setw(3)
                       << hit->GetPaddleNumber() << setw(10) << hit->GetPosTDC() * fScinTdcToTime
                       << setw(10) << fTOFPInfo[ih].pathp << setw(10) << fTOFPInfo[ih].zcor
                       << setw(10) << fTOFPInfo[ih].time_pos << setw(10) << hit->GetPosADC()
                       << endl;
              fDumpOut << showpoint << " 2" << setw(3) << ip + 1 << setw(3)
                       << hit->GetPaddleNumber() << setw(10) << hit->GetNegTDC() * fScinTdcToTime
                       << setw(10) << fTOFPInfo[ih].pathn << setw(10) << fTOFPInfo[ih].zcor
                       << setw(10) << fTOFPInfo[ih].time_neg << setw(10) << hit->GetNegADC()
                       << endl;
            }
            Int_t padind = ((THcHodoHit*)hodoHits->At(iphit))->GetPaddleNumber() - 1;
            pl_xypos += fPlanes[ip]->GetPosCenter(padind) + fPlanes[ip]->GetPosOffset();
            pl_zpos += fPlanes[ip]->GetZpos() + (padind % 2) * fPlanes[ip]->GetDzpos();
            num_good_pad++;
          }
          ih++;
          //	  cout << ip << " " << iphit << " " <<  fGoodFlags[itrk][ip][iphit].goodScinTime <<
          //" " <<   fGoodFlags[itrk][ip][iphit].goodTdcPos << " " <<
          // fGoodFlags[itrk][ip][iphit].goodTdcNeg << endl;
        }
        hitDistance = kBig;
        if (num_good_pad != 0) {
          pl_xypos = pl_xypos / num_good_pad;
          pl_zpos  = pl_zpos / num_good_pad;
          hitPos   = theTrack->GetY() + theTrack->GetPhi() * pl_zpos;
          if (ip % 2 == 0)
            hitPos = theTrack->GetX() + theTrack->GetTheta() * pl_zpos;
          hitDistance = hitPos - pl_xypos;
          fPlanes[ip]->SetTrackXPosition(theTrack->GetX() + theTrack->GetTheta() * pl_zpos);
          fPlanes[ip]->SetTrackYPosition(theTrack->GetY() + theTrack->GetPhi() * pl_zpos);
        }
        //      cout << " ip " << ip << " " << hitPos << " " << pl_xypos << " " << pl_zpos << " "
        //      << hitDistance << endl;
        fPlanes[ip]->SetHitDistance(hitDistance);
      }
      if (ih > 0 && fDumpTOF && Ntracks == 1 && fGoodEventTOFCalib &&
          shower_track_enorm > fTOFCalib_shtrk_lo && shower_track_enorm < fTOFCalib_shtrk_hi) {
        fDumpOut << "0 " << endl;
      }
    }
  } // over tracks
  //
  return 0;
}
//_____________________________________________________________________________
Int_t THcHodoscope::GetScinIndex(Int_t nPlane, Int_t nPaddle) {
  // GN: Return the index of a scintillator given the plane # and the paddle #
  // This assumes that both planes and
  // paddles start counting from 0!
  // Result also counts from 0.
  return fNPlanes * nPaddle + nPlane;
}
//_____________________________________________________________________________
Int_t THcHodoscope::GetScinIndex(Int_t nSide, Int_t nPlane, Int_t nPaddle) {
  return nSide * fMaxHodoScin + fNPlanes * nPaddle + nPlane - 1;
}
//_____________________________________________________________________________
Double_t THcHodoscope::GetPathLengthCentral() { return fPathLengthCentral; }

//_____________________________________________________________________________
Int_t THcHodoscope::End(THaRunBase* run) {
  MissReport(Form("%s.%s", GetApparatus()->GetName(), GetName()));
  return 0;
}
ClassImp(THcHodoscope)
    ////////////////////////////////////////////////////////////////////////////////
