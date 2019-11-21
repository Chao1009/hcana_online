#include "ConfigOption.h"
#include "PRadETChannel.h"
#include "et.h"
#include "evio/evioUtil.hxx"
#include "evio/evioFileChannel.hxx"
#include <csignal>
#include <thread>
#include <chrono>
#include <iostream>

#define PROGRESS_COUNT 100

using namespace std::chrono;


volatile std::sig_atomic_t gSignalStatus;


void signal_handler(int signal) {
    gSignalStatus = signal;
}


int main(int argc, char* argv[]) try
{
    // setup input arguments
    ConfigOption conf_opt;
    conf_opt.AddLongOpt(ConfigOption::help_message, "help");
    conf_opt.AddOpt(ConfigOption::arg_require, 'h');
    conf_opt.AddOpt(ConfigOption::arg_require, 'p');
    conf_opt.AddOpt(ConfigOption::arg_require, 'f');
    conf_opt.AddOpt(ConfigOption::arg_require, 'i');

    conf_opt.SetDesc("usage: %0 <data_file>");
    conf_opt.SetDesc('h', "host address of the ET system, default \"localhost\".");
    conf_opt.SetDesc('p', "port to connect, default 11111.");
    conf_opt.SetDesc('f', "memory mapped et file, default \"/tmp/et_feeder\".");
    conf_opt.SetDesc('i', "interval in milliseconds to write data, default \"10\"");

    if (!conf_opt.ParseArgs(argc, argv) || conf_opt.NbofArgs() != 1) {
        std::cout << conf_opt.GetInstruction() << std::endl;
        return -1;
    }

    std::string host = "localhost";
    int port = 11111;
    std::string etf = "/tmp/et_feeder";
    int interval = 10;

    for (auto &opt : conf_opt.GetOptions()) {
        switch (opt.mark) {
        case 'h':
            host = opt.var.String();
            break;
        case 'c':
            port = opt.var.Int();
            break;
        case 'f':
            etf = opt.var.String();
            break;
        case 'i':
            interval = opt.var.Int();
            break;
        default :
            std::cout << conf_opt.GetInstruction() << std::endl;
            return -1;
        }
    }

    // attach to ET system
    auto ch = new PRadETChannel();
    ch->Open(host.c_str(), port, etf.c_str());
    ch->NewStation("Data Feeder");
    ch->AttachStation();

    // evio file reader
    evio::evioFileChannel *chan = new evio::evioFileChannel(conf_opt.GetArgument(0).c_str(), "r");
    chan->open();

    // install signal handler
    std::signal(SIGINT, signal_handler);
    int count = 0;
    while (chan->read()) {
        if (gSignalStatus == SIGINT) {
            std::cout << "Received control-C, exiting..." << std::endl;
            ch->ForceClose();
            break;
        }
        system_clock::time_point start(system_clock::now());
        system_clock::time_point next(start + std::chrono::milliseconds(interval));

        if (++count % PROGRESS_COUNT == 0) {
            std::cout << "Read and feed " << count << " events to ET, rate is 1 event per "
                      << interval << " ms.\r" << std::flush;
        }
        ch->Write((void*) chan->getBuffer(), chan->getBufSize() * sizeof(uint32_t));

        std::this_thread::sleep_until(next);
    }
    std::cout << "Read and feed " << count << " events to ET, rate is 1 event per "
              << interval << " ms." << std::endl;

    chan->close();
    return 0;

} catch (PRadException e) {
    std::cerr << e.FailureType() << ": " << e.FailureDesc() << std::endl;
    return -1;
} catch (evio::evioException e) {
    std::cerr << e.toString() << endl;
} catch (...) {
    std::cerr << "?unknown exception" << endl;
}

