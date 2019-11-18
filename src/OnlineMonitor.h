#ifndef hcana_OnlineMonitor_h_
#define hcana_OnlineMonitor_h_

#include "THaBenchmark.h"
#include "THcAnalyzer.h"
#include <thread>
#include <chrono>
#include <iostream>


class PRadETChannel;

namespace hcana {

    class OnlineMonitor : public THcAnalyzer {
    public:
        OnlineMonitor() : THcAnalyzer() {}
        virtual ~OnlineMonitor() {}

        virtual Int_t Monitor(PRadETChannel *ch, std::chrono::seconds interval = std::chrono::seconds(10));
        virtual Int_t ReadOnce(PRadETChannel *ch, size_t max_events = 10000);
        Int_t ReadBuffer(uint32_t *buf);
        Int_t ProcOneEvent();
        //Int_t GoToEndOfCodaFile();

        ClassDef(OnlineMonitor, 0) // Hall C Analyzer Standard Event Loop
    private:
        bool fMonitor;
    };

} // namespace hcana
#endif
