#ifndef hcana_Scandalizer_h_
#define hcana_Scandalizer_h_

#include "THaBenchmark.h"
#include "THcAnalyzer.h"
#include "PRadETChannel.h"
#include <iostream>


namespace hcana {

  class Scandalizer : public THcAnalyzer {
  public:
    Scandalizer() : THcAnalyzer() {}
    virtual ~Scandalizer() {}

    virtual Int_t Process(THaRunBase* run = nullptr);
    virtual Int_t ReadOneEvent();
    //Int_t GoToEndOfCodaFile();
    void SetETChannel(PRadETChannel *ch) { online_ch = ch; }
    PRadETChannel *GetETChannel() { return online_ch; }

    int _skip_events = 0;

    PRadETChannel *online_ch;

    ClassDef(Scandalizer, 0) // Hall C Analyzer Standard Event Loop
  };

} // namespace hcana
#endif
