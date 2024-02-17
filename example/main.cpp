#include <iostream>

#include "args/args.h"

int main(int argc, char** argv) {
    using namespace Args;

    struct {
        std::string rom {};
        bool serial {};
        float scaling {};
        bool dumpCartridgeInfo {};
    } args;

    Parser parser;
    parser.addArgument(args.rom, "rom").help("ROM");
    parser.addArgument(args.serial, "--serial", "-s").help("Display serial console");
    parser.addArgument(args.scaling, "--scaling", "-z").help("Scaling factor");
    parser.addArgument(args.dumpCartridgeInfo, "--cartridge-info", "-i").help("Dump cartridge info and quit");

    if (!parser.parse(argc, argv, 1))
        return 1;

    std::cout << "rom               = " << args.rom << std::endl;
    std::cout << "serial            = " << args.serial << std::endl;
    std::cout << "scaling           = " << args.scaling << std::endl;
    std::cout << "dumpCartridgeInfo = " << args.dumpCartridgeInfo << std::endl;
}
