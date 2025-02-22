#include <iostream>

#include "args/args.h"

int main(int argc, char** argv) {
    using namespace Args;

    struct {
        std::string rom {};
        std::optional<std::string> second_rom {};
        bool serial {};
        float scaling {};
        bool dump_cartridge_info {};
    } args;

    Parser parser;
    parser.add_argument(args.rom, "rom").help("ROM");

    parser.add_argument(args.second_rom, "--second-rom", "-2").help("Second rom");

    parser.add_argument(args.serial, "--serial", "-s").help("Display serial console");
    //     parser.add_argument(args.serial, "--serial", "-s").required(true).help("Display serial console");

    parser.add_argument(args.scaling, "--scaling", "-z").help("Scaling factor");

    parser.add_argument(args.dump_cartridge_info, "--cartridge-info", "-i").help("Dump cartridge info and quit");

    if (!parser.parse(argc, argv, 1)) {
        return 1;
    }

    std::cout << "rom                 = " << args.rom << std::endl;
    std::cout << "second_rom          = " << args.second_rom.value_or("<nullopt>") << std::endl;
    std::cout << "serial              = " << args.serial << std::endl;
    std::cout << "scaling             = " << args.scaling << std::endl;
    std::cout << "dump_cartridge_info = " << args.dump_cartridge_info << std::endl;
}
