#include "OnlineMonitor.h"
#include "PRadETChannel.h"

using namespace std::chrono;


namespace hcana {

Int_t OnlineMonitor::Monitor(PRadETChannel *ch, std::chrono::seconds interval)
{
    Int_t total = 0;
    fMonitor = true;
    static const char* const here = "Monitor";

    fBench->Begin("Total");

    void (*prev_handler)(int);
    prev_handler = signal(SIGINT, handle_sig);

    while (!fMonitor) {
        system_clock::time_point start(system_clock::now());
        system_clock::time_point next(start + interval);

        total += ReadOnce(ch);
        std::this_thread::sleep_until(next);
    }

    signal(SIGINT, prev_handler);

    if (fDoBench)
        fBench->Begin("Output");

    if (fOutput && fOutput->GetTree()) {
        fFile = fOutput->GetTree()->GetCurrentFile();
    }
    if (fFile)
        fFile->cd();
    if (fOutput)
        fOutput->End();
    if (fFile) {
        fRun->Write("Run Data");
        fFile->Purge();
    }
    if (fDoBench)
        fBench->Stop("Output");

    return total;
}


Int_t OnlineMonitor::ReadOnce(PRadETChannel *ch, size_t max_events)
try {
    Int_t count = 0;
    for(size_t num = 0; ch->Read() && (num < max_events); ++num) {
        auto buf = ch->GetBuffer();
        count += ReadBuffer((uint32_t*)(ch->GetBuffer()));
    }
    return count;
}
catch (PRadException e) {
    std::cerr << e.FailureType() << ": " << e.FailureDesc() << std::endl;
    fMonitor = false;
    return 0;
}


Int_t OnlineMonitor::ReadBuffer(uint32_t *buf)
try {
    switch (fEvData->LoadEvent(buf)) {
    case THaEvData::HED_WARN:
    case THaEvData::HED_OK:
        break;
    default:
        return 0;
    }

    Int_t count = 0;
    do {
        count += ProcOneEvent();
    } while ( fEvData->IsMultiBlockMode() &&
              !fEvData->BlockIsDone() &&
              (fEvData->LoadFromMultiBlock() == THaEvData::HED_OK) );

    return count;
}
catch (...) {
    throw;
}

Int_t OnlineMonitor::ProcOneEvent()
{
    UInt_t evnum = fEvData->GetEvNum();

    switch (fCountMode) {
    case kCountPhysics:
        if (fEvData->IsPhysicsTrigger())
            fNev++;
        break;
    case kCountAll:
        fNev++;
        break;
    case kCountRaw:
        fNev = evnum;
        break;
    default:
        break;
    }

    if (fUpdateRun)
        fRun->Update(fEvData);

    if (fDoBench)
        fBench->Begin("Cuts");
    gHaCuts->ClearAll();
    if (fDoBench)
        fBench->Stop("Cuts");

    switch (MainAnalysis()) {
    case kOK:
        return 1;
    case kSkip:
    default:
        break;
    case kTerminate:
        throw(PRadException("Termination", "analysis is terminated."));
    case kFatal:
        throw(PRadException("Fatal Error", "analysis encountered fatal error."));
    }

    return 0;
}

} // namespace hcana
