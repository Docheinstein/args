## Args

Simple C++ library for parse command line arguments, in Python's argparse style.

Automatically present the help message if no arguments or  `-h` or `--help` is given.

### Example

In you can find an usage example in `main.cpp`.

A minimal example:

```cpp
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

// You can use `args` here
```

The produced help message for the example above:

```
usage: rom [--serial] [--scaling SCALING] [--cartridge-info] [--help]

positional arguments:
  rom                    ROM

optional arguments:
  -s, --serial           Display serial console
  -z, --scaling SCALING  Scaling factor
  -i, --cartridge-info   Dump cartridge info and quit
  -h, --help             Display this help message and quit
```

### Usage

To use Args as a static library with CMake, copy it or add it as a submodule,
then add to your `CMakeLists.txt`:

```
add_executable(my-awesome-project STATIC)

...

add_subdirectory(args)

target_link_libraries(my-awesome-project PRIVATE args)
```