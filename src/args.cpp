#include "args/args.h"
#include <algorithm>
#include <complex>
#include <iostream>
#include <optional>

namespace Args {

namespace {
    /*
     * Breaks the given string after maxWidth so that it
     * will always begin from colWidth for the consecutive rows.
     *
     *  <-----maxWidth------>
     *  <-colWidth->
     *  |-----------|-------|
     *  |           |-------|
     *  |           |-------|
     */
    std::string wrap(const std::string& s, unsigned int colWidth, unsigned int maxWidth) {
        static const char* WHITESPACES = " \f\n\r\t\v";

        std::string out {};
        std::string row {};
        std::size_t begin {}, end {};

        // Add inital white spaces
        begin = s.find_first_not_of(WHITESPACES);
        if (begin == std::string::npos) {
            // There's nothing useful in the string
            return {};
        }
        out += s.substr(0, begin);

        do {
            // Identify the next token
            const auto firstSpaceAfterToken = s.find_first_of(WHITESPACES, begin);
            end = s.find_first_not_of(WHITESPACES, firstSpaceAfterToken < std::string::npos ? firstSpaceAfterToken + 1
                                                                                            : std::string::npos);
            const std::string token = s.substr(begin, end - begin);
            const auto tokenSize =
                firstSpaceAfterToken < std::string::npos ? (firstSpaceAfterToken - begin) : s.size() - begin;
            begin = end;

            // Append it to the current row, if it fits
            if (row.size() + tokenSize < maxWidth) {
                // The token fits this row
                row += token;
            } else {
                // The token does not fit this row: append the current row to
                // the output and proceed by appending this token to the next row
                out += row + "\n";
                row = std::string(colWidth, ' ') + token;
            }
        } while (end != std::string::npos);

        // Eventually add the remaining part of the last row
        out += row;

        return out;
    }
} // namespace

ArgumentParseFeed::ArgumentParseFeed(const std::vector<std::string>& argv, std::vector<std::string>& errors,
                                     unsigned int index) :
    argv {argv},
    errors {errors},
    index {index} {
}

bool ArgumentParseFeed::hasNext(unsigned int n) const {
    return index + n <= argv.size();
}

const std::string& ArgumentParseFeed::seekNext() const {
    return argv[index];
}

const std::string& ArgumentParseFeed::popNext() {
    return argv[index++];
}

void ArgumentParseFeed::addError(std::string&& error) const {
    errors.emplace_back(std::move(error));
}

ArgumentConfig::ArgumentConfig(std::vector<std::string>&& names) :
    names {std::move(names)} {
}

ArgumentConfig& ArgumentConfig::help(const std::string& h) {
    help_ = h;
    return *this;
}

Argument::Argument(std::vector<std::string>&& names) :
    ArgumentConfig {std::move(names)} {
}

Parser::Parser() {
    // Add the help argument by default
    addArgument(helpRequest, "--help", "-h").help("Display this help message and quit");
}

bool Parser::parse(unsigned int argc, char** argv, unsigned int from) {
    const auto printErrors = [](const std::vector<std::string>& errors) {
        for (const auto& error : errors) {
            std::cerr << "ERROR: " << error << std::endl;
        }
    };

    // Quit immediately if the parser is not properly setup
    if (!setupErrors.empty()) {
        printErrors(setupErrors);
        return false;
    }

    // Build the args vector
    std::vector<std::string> args {};
    for (unsigned int i = from; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    // Print the help if no argument is given
    if (args.empty()) {
        printHelp();
        return false;
    }

    // Clear any previous parse error
    parseErrors.clear();

    // Actually start parse
    ArgumentParseFeed feed {args, parseErrors};

    unsigned int positionalIndex = 0;

    while (feed.hasNext() && parseErrors.empty()) {
        // Pop next token
        const auto& token = feed.seekNext();

        // Check whether it is an optional argument
        if (const auto it = optionals.find(token); it != optionals.end()) {
            // It's a known optional argument
            auto* const arg = it->second;

            // Consume the token
            feed.popNext();

            // Verify that there are enough tokens for this argument
            if (feed.hasNext(arg->numParams())) {
                arg->parse(feed);
            } else {
                feed.addError("missing parameter for argument '" + token + "'");
            }
        } else if (positionalIndex < positionals.size()) {
            // It's a positional argument we still have to read
            positionals[positionalIndex++]->parse(feed);
        } else {
            // Neither a positional or a known optional: throw an error
            parseErrors.emplace_back("unknown argument '" + token + "'");
        }
    }

    // Print the help if either '-h' or '--help' is given.
    if (helpRequest) {
        helpRequest = false;
        printHelp();
        return false;
    }

    // Check if we are still missing some (mandatory) positional argument
    if (positionalIndex != positionals.size()) {
        for (unsigned int i = positionalIndex; i < positionals.size(); i++) {
            parseErrors.emplace_back("missing positional argument '" + positionals[i]->names[0] + "'");
        }
    }

    // Eventuall dump parse errors
    if (!parseErrors.empty()) {
        printErrors(parseErrors);
        return false;
    }

    // Everything is ok
    return true;
}

void Parser::printHelp() const {
    static constexpr int MAX_WIDTH = 80;

    // Helper data structures

    struct UsageEntry {
        std::string name {};
        std::optional<std::string> paramName {};
        bool optional {};
    };

    struct PositionalEntry {
        std::string name {};
        std::optional<std::string> help {};
    };

    struct OptionalEntry {
        std::vector<std::string> names {};
        std::optional<std::string> paramName {};
        std::optional<std::string> help {};
    };

    // Helper functions

    const auto isHelpArgument = [](const Argument* arg) {
        return arg->names[0] == "--help";
    };

    const auto isOptionalArgument = [](const Argument* arg) {
        return arg->names[0][0] == '-';
    };

    const auto findArgumentLongestName = [](const Argument* arg) {
        return *std::max_element(arg->names.begin(), arg->names.end(),
                                 [](const std::string& s1, const std::string& s2) {
                                     return s1.size() < s2.size();
                                 });
    };

    std::vector<UsageEntry> usage {};
    std::vector<PositionalEntry> positionals {};
    std::vector<OptionalEntry> optionals {};

    unsigned int argsColWidth = 0;

    // Sort the arguments so that all the positionals precede the optionals (and help is last)
    std::vector<Argument*> sortedArguments {};
    sortedArguments.resize(arguments.size());
    std::transform(arguments.begin(), arguments.end(), sortedArguments.begin(),
                   [](const std::unique_ptr<Argument>& arg) {
                       return &*arg;
                   });
    std::sort(sortedArguments.begin(), sortedArguments.end(),
              [isOptionalArgument, isHelpArgument](const Argument* a1, const Argument* a2) {
                  // Help is always last
                  if (isHelpArgument(a1) != isHelpArgument(a2))
                      return isHelpArgument(a2);

                  // The positionals precede the optionals
                  return isOptionalArgument(a1) < isOptionalArgument(a2);
              });

    // Iterate all the arguments and fill the data structures
    for (const auto* arg : sortedArguments) {
        // Find the primary name of this argument (i.e. the longest one)
        const std::string& primaryName = findArgumentLongestName(arg);

        // Find out if this is an optional or a positional argument
        const bool isOptional = isOptionalArgument(arg);

        // Compute the parameter name as the primary name without leading dashes upper case
        std::optional<std::string> paramName {};
        if (isOptional && arg->numParams()) {
            std::string s = primaryName.substr(primaryName.find_first_not_of('-'));
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return std::toupper(c);
            });
            paramName = s;
        }

        // Add the usage entry
        usage.push_back({primaryName, paramName, isOptional});

        if (isOptional) {
            // Add the optional entry
            std::vector<std::string> sortedNames = arg->names;
            std::sort(sortedNames.begin(), sortedNames.end(), std::greater<>());
            optionals.push_back({sortedNames, paramName, arg->help_});
        } else {
            // Add the positional entry
            positionals.push_back({primaryName, arg->help_});
        }

        // Update argsColWidth with the known maximum of all the arguments' name + param strings
        unsigned argColWidth = 0;
        std::for_each(arg->names.begin(), arg->names.end(), [&argColWidth](const std::string& s) {
            argColWidth += s.size() + 2 /* comma + space */;
        });
        argColWidth += paramName ? (paramName->size() + 1 /* space */) : 0;
        argsColWidth = std::max(argsColWidth, argColWidth);
    }

    // Print usage
    {
        std::stringstream ss {};
        for (unsigned int i = 0; i < usage.size(); i++) {
            const auto& [name, paramName, optional] = usage[i];
            ss << (optional ? "[" : "");
            ss << name << (paramName ? (" " + *paramName) : "");
            ss << (optional ? "]" : "");
            ss << ((i < usage.size() - 1) ? " " : "");
        }

        std::cout << "usage: " << wrap(ss.str(), 7, MAX_WIDTH) << std::endl << std::endl;
    }

    static const std::string pad = "  ";

    // Print positional arguments
    {
        std::cout << "positional arguments:" << std::endl;
        for (const auto& [name, help] : positionals) {
            std::stringstream ss;
            ss << pad << std::left << std::setw(static_cast<int>(argsColWidth)) << name << help.value_or("");
            std::cout << wrap(ss.str(), argsColWidth + pad.size(), MAX_WIDTH) << std::endl;
        }
        std::cout << std::endl;
    }

    // Print optional arguments
    {
        std::cout << "optional arguments:" << std::endl;
        for (const auto& [names, paramName, help] : optionals) {
            std::stringstream ss1;
            for (unsigned int i = 0; i < names.size(); i++) {
                ss1 << names[i] << ((i < names.size() - 1) ? ", " : "");
            }
            ss1 << (paramName ? (" " + *paramName) : "");

            std::stringstream ss2;
            ss2 << pad << std::left << std::setw(static_cast<int>(argsColWidth)) << ss1.str() << help.value_or("");
            std::cout << wrap(ss2.str(), argsColWidth + pad.size(), MAX_WIDTH) << std::endl;
        }
    }
}
} // namespace Args
