#include "ConfigOption.h"
#include "PRadETChannel.h"
#include "et.h"
#include "evio.h"

#define PROGRESS_COUNT 1000


int main(int argc, char* argv[])
{
    // setup input arguments
    ConfigOption conf_opt;
    conf_opt.AddLongOpt(ConfigOption::help_message, "help");
    conf_opt.AddOpt(ConfigOption::arg_require, 'h');
    conf_opt.AddOpt(ConfigOption::arg_require, 'p');
    conf_opt.AddOpt(ConfigOption::arg_require, 'f');
    conf_opt.AddOpt(ConfigOption::arg_require, 'r');

    conf_opt.SetDesc("usage: %0 <data_file>");
    conf_opt.SetDesc('h', "host address of the ET system, default \"localhost\".");
    conf_opt.SetDesc('p', "port to connect, default 11111.");
    conf_opt.SetDesc('f', "memory mapped et file, default \"/tmp/et_feeder\".");
    conf_opt.SetDesc('r', "rate in mili-seconds to write data, default \"10\"");

    if (!conf_opt.ParseArgs(argc, argv) || conf_opt.NbofArgs() != 2) {
        std::cout << conf_opt.GetInstruction() << std::endl;
        return -1;
    }

    std::string host = "localhost";
    int port = 11111;
    std::string etf = "/tmp/et_feeder";
    float rate = 10.;

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
        case 'r':
            rate = opt.var.Float();
            break;
        default :
            std::cout << conf_opt.GetInstruction() << std::endl;
            return -1;
        }
    }
}

