#include "ConfigOption.h"
#include "PRadETChannel.h"
#include "et.h"
#include "evio.h"
#include <csignal>
#include <thread>
#include <chrono>
#include <iostream>


using namespace std::chrono;


volatile std::sig_atomic_t gSignalStatus;


void signal_handler(int signal) {
    gSignalStatus = signal;
}


int main(int argc, char* argv[])
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

    auto ch = new PRadETChannel();
    try {
        ch->Open(host.c_str(), port, etf.c_str());
        ch->NewStation("Data Feeder");
        ch->AttachStation();
    } catch (PRadException e) {
        std::cerr << e.FailureType() << ": " << e.FailureDesc() << std::endl;
        return -1;
    }

    // install signal handler
    std::signal(SIGINT, signal_handler);
    while (true) {
        if (gSignalStatus == SIGINT) {
            std::cout << "Received control-C, exiting..." << std::endl;
            ch->ForceClose();
            break;
        }
        system_clock::time_point start(system_clock::now());
        system_clock::time_point next(start + std::chrono::milliseconds(interval));

        std::this_thread::sleep_until(next);
    }

    return 0;
}

