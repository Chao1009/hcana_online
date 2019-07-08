// Preamble to HallC_LinkDef.h file

#ifdef __CINT__
 
#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;
 
#pragma link C++ nestedclass;
#pragma link C++ nestedtypedef;

#pragma link C++ namespace hallc;
#pragma link C++ namespace hcana;
#pragma link C++ namespace podd2;

#pragma link C++ namespace hallc::data;

#pragma link C++ class hallc::data::Hodoscope+;
#pragma link C++ class hallc::data::DriftChamber+;

#pragma link C++ class hallc::data::PulseWaveForm+;
#pragma link C++ class std::vector<hallc::data::PulseWaveForm>+;


#pragma link C++ class podd2::HitLogging<TObject>+;

#pragma link C++ global gHcParms;
#pragma link C++ global gHcDetectorMap;
 

#pragma link C++ class Decoder::Scaler9001+;
#pragma link C++ class Decoder::Scaler9250+;
#pragma link C++ class THcAerogel+;
#pragma link C++ class THcAerogelHit+;
#pragma link C++ class THcAnalyzer+;
#pragma link C++ class THcBCMCurrent+;
#pragma link C++ class THcCherenkov+;
#pragma link C++ class THcCherenkovHit+;
#pragma link C++ class THcCoinTime+;
#pragma link C++ class THcConfigEvtHandler+;
#pragma link C++ class THcDC+;
#pragma link C++ class THcDCHit+;
#pragma link C++ class THcDCLookupTTDConv+;
#pragma link C++ class THcDCTimeToDistConv+;
#pragma link C++ class THcDCTrack+;
#pragma link C++ class THcDCWire+;
#pragma link C++ class THcDetectorMap+;
#pragma link C++ class THcDriftChamber+;
#pragma link C++ class THcDriftChamberPlane+;
#pragma link C++ class THcDummySpectrometer+;
#pragma link C++ class THcExtTarCor+;
#pragma link C++ class THcFormula+;
#pragma link C++ class THcHallCSpectrometer+;
#pragma link C++ class THcHitList+;
#pragma link C++ class THcHodoEff+;
#pragma link C++ class THcHelicity+;
#pragma link C++ class THcHelicityReader+;
#pragma link C++ class THcHodoHit+;
#pragma link C++ class THcHodoscope+;
#pragma link C++ class THcInterface+;
#pragma link C++ class THcParmList+;
#pragma link C++ class THcPeriodicReport+;
#pragma link C++ class THcPrimaryKine+;
#pragma link C++ class THcRaster+;
#pragma link C++ class THcRasteredBeam+;
#pragma link C++ class THcRasterRawHit+;
#pragma link C++ class THcRawAdcHit+;
#pragma link C++ class THcRawDCHit+;
#pragma link C++ class THcRawHit+;
#pragma link C++ class THcRawHodoHit+;
#pragma link C++ class THcRawShowerHit+;
#pragma link C++ class THcRawTdcHit+;
#pragma link C++ class THcReactionPoint+;
#pragma link C++ class THcRun+;
#pragma link C++ class THcRunParameters+;
#pragma link C++ class THcScalerEvtHandler+;
#pragma link C++ class THcScintillatorPlane+;
#pragma link C++ class THcScintPlaneCluster+;
#pragma link C++ class THcSecondaryKine+;
#pragma link C++ class THcShowerArray+;
#pragma link C++ class THcShower+;
#pragma link C++ class THcShowerHit+;
#pragma link C++ class THcShowerPlane+;
#pragma link C++ class THcSignalHit+;
#pragma link C++ class THcSpacePoint+;
#pragma link C++ class THcTimeSyncEvtHandler+;
#pragma link C++ class THcTrigApp+;
#pragma link C++ class THcTrigDet+;
#pragma link C++ class THcTrigRawHit+;
#pragma link C++ class Decoder::TIBlobModule+;

#pragma link C++ class hcana::Shower2+;
#pragma link C++ class hcana::Scandalizer+;
#pragma link C++ class hcana::TrackingEfficiency+;

// Postamble for HallC_Linkdef.h file
#endif
