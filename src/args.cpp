#include "args/args.h"
#include <algorithm>
#include <complex>
#include <iostream>
#include <optional>
#include <set>

namespace Args {

namespace {
    /*
     * Breaks the given string after max_width so that it
     * will always begin from col_width for the consecutive rows.
     *
     *  <-----max_width------>
     *  <-col_width->
     *  |-----------|-------|
     *  |           |-------|
     *  |           |-------|
     */
    std::string wrap(const std::string& s, unsigned int col_width, unsigned int max_width) {
        static const char* whitespaces = " \f\n\r\t\v";

        std::string out {};
        std::string row {};
        std::size_t begin {}, end {};

        // Add inital white spaces
        begin = s.find_first_not_of(whitespaces);
        if (begin == std::string::npos) {
            // There's nothing useful in the string
            return {};
        }
        out += s.substr(0, begin);

        do {
            // Identify the next token
            const auto first_space_after_token = s.find_first_of(whitespaces, begin);
            end = s.find_first_not_of(whitespaces, first_space_after_token < std::string::npos
                                                       ? first_space_after_token + 1
                                                       : std::string::npos);
            const std::string token = s.substr(begin, end - begin);
            const auto token_size =
                first_space_after_token < std::string::npos ? (first_space_after_token - begin) : s.size() - begin;
            begin = end;

            // Append it to the current row, if it fits
            if (row.size() + token_size < max_width) {
                // The token fits this row
                row += token;
            } else {
                // The token does not fit this row: append the current row to
                // the output and proceed by appending this token to the next row
                out += row + "\n";
                row = std::string(col_width, ' ') + token;
            }
        } while (end != std::string::npos);

        // Eventually add the remaining part of the last row
        out += row;

        return out;
    }
} // namespace

ArgumentParseContext::ArgumentParseContext(const std::vector<std::string>& argv, std::vector<std::string>& errors,
                                           unsigned int index) :
    argv {argv},
    errors {errors},
    index {index} {
}

bool ArgumentParseContext::has_next(unsigned int n) const {
    return index + n <= argv.size();
}

const std::string& ArgumentParseContext::seek_next() const {
    return argv[index];
}

const std::string& ArgumentParseContext::pop_next() {
    return argv[index++];
}

void ArgumentParseContext::add_error(std::string&& error) const {
    errors.emplace_back(std::move(error));
}

ArgumentConfig::ArgumentConfig(std::vector<std::string>&& names) :
    names {std::move(names)} {
}

ArgumentConfig& ArgumentConfig::required(bool req) {
    required_ = req;
    return *this;
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
    add_argument(help_request, "--help", "-h").help("Display this help message and quit");
}

bool Parser::parse(unsigned int argc, char** argv, unsigned int from) {
    const auto print_errors = [](const std::vector<std::string>& errors) {
        for (const auto& error : errors) {
            std::cerr << "ERROR: " << error << std::endl;
        }
    };

    // Quit immediately if the parser is not properly setup
    if (!setup_errors.empty()) {
        print_errors(setup_errors);
        return false;
    }

    // Build the args vector
    std::vector<std::string> args {};
    for (unsigned int i = from; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    // Clear any previous parse error
    parse_errors.clear();

    // Actually start parse
    ArgumentParseContext context {args, parse_errors};

    std::set<Argument*> parsed_args {};

    unsigned int positional_index = 0;

    while (context.has_next() && parse_errors.empty()) {
        // Pop next token
        const auto& token = context.seek_next();

        // Check whether it is an option
        if (const auto it = options.find(token); it != options.end()) {
            // It's a known option
            auto* const arg = it->second;

            // Consume the token
            context.pop_next();

            // Verify that there are enough tokens for this argument
            if (context.has_next(arg->num_min_params())) {
                arg->parse(context);
                parsed_args.emplace(arg);
            } else {
                context.add_error("missing parameter for argument '" + token + "'");
            }
        } else if (positional_index < positionals.size()) {
            // It's a positional argument we still have to read
            auto* const arg = positionals[positional_index++];
            arg->parse(context);
            parsed_args.emplace(arg);
        } else {
            // Neither a positional nor a known option: throw an error
            parse_errors.emplace_back("unknown argument '" + token + "'");
        }
    }

    // Print the help if either '-h' or '--help' is given.
    if (help_request) {
        help_request = false;
        print_help();
        return false;
    }

    // Check if we are missing some (required) argument
    for (const auto& arg : arguments) {
        if (arg->required_ && parsed_args.find(&*arg) == parsed_args.end()) {
            parse_errors.emplace_back("missing required argument '" + arg->names[0] + "'");
        }
    }

    // Eventually dump parse errors
    if (!parse_errors.empty()) {
        print_errors(parse_errors);
        return false;
    }

    // Everything is ok
    return true;
}

void Parser::print_help() const {
    static constexpr int max_width = 80;

    // Helper data structures

    struct UsageEntry {
        std::string name {};
        bool optional {};
        struct {
            std::optional<std::string> name {};
            bool optional {};
        } param;
    };

    struct PositionalEntry {
        std::string name {};
        std::optional<std::string> help {};
    };

    struct OptionEntry {
        std::vector<std::string> names {};
        std::optional<std::string> param_name {};
        std::optional<std::string> help {};
    };

    // Helper functions

    const auto is_help_argument = [](const Argument* arg) {
        return arg->names[0] == "--help";
    };

    const auto is_option_argument = [](const Argument* arg) {
        return arg->names[0][0] == '-';
    };

    const auto find_argument_longest_name = [](const Argument* arg) {
        return *std::max_element(arg->names.begin(), arg->names.end(),
                                 [](const std::string& s1, const std::string& s2) {
                                     return s1.size() < s2.size();
                                 });
    };

    std::vector<UsageEntry> usage {};
    std::vector<PositionalEntry> positional_entries {};
    std::vector<OptionEntry> option_entries {};

    unsigned int args_col_width = 0;

    // Sort the arguments so that all the positionals precede the options (and help is last)
    std::vector<Argument*> sorted_arguments {};
    sorted_arguments.resize(arguments.size());
    std::transform(arguments.begin(), arguments.end(), sorted_arguments.begin(),
                   [](const std::unique_ptr<Argument>& arg) {
                       return &*arg;
                   });
    std::sort(sorted_arguments.begin(), sorted_arguments.end(),
              [is_option_argument, is_help_argument](const Argument* a1, const Argument* a2) {
                  // Help is always last
                  if (is_help_argument(a1) != is_help_argument(a2))
                      return is_help_argument(a2);

                  // The positionals precede the options
                  return is_option_argument(a1) < is_option_argument(a2);
              });

    // Iterate all the arguments and fill the data structures
    for (const auto* arg : sorted_arguments) {
        // Find the primary name of this argument (i.e. the longest one)
        const std::string& primary_name = find_argument_longest_name(arg);

        // Find out if this is an option or a positional argument from its name
        const bool is_option = is_option_argument(arg);

        // Find out if it's optional or mandatory
        const bool is_optional = !arg->required_;
        const bool is_param_optional = is_option && arg->num_max_params() > arg->num_min_params();

        // Compute the parameter name as the primary name without leading dashes upper case
        std::optional<std::string> param_name {};
        if (is_option && arg->num_max_params()) {
            std::string s = primary_name.substr(primary_name.find_first_not_of('-'));
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return std::toupper(c);
            });
            param_name = s;
        }

        // Add the usage entry
        usage.push_back({primary_name, is_optional, {param_name, is_param_optional}});

        if (is_option) {
            // Add the option
            std::vector<std::string> sorted_names = arg->names;
            std::sort(sorted_names.begin(), sorted_names.end(), std::greater<>());
            option_entries.push_back({sorted_names, param_name, arg->help_});
        } else {
            // Add the positional entry
            positional_entries.push_back({primary_name, arg->help_});
        }

        // Update args_col_width with the known maximum of all the arguments' name + param strings
        unsigned arg_col_width = 0;
        std::for_each(arg->names.begin(), arg->names.end(), [&arg_col_width](const std::string& s) {
            arg_col_width += s.size() + 2 /* comma + space */;
        });
        arg_col_width += param_name ? (param_name->size() + 1 /* space */) : 0;
        args_col_width = std::max(args_col_width, arg_col_width);
    }

    // Print usage
    {
        std::stringstream ss {};
        for (unsigned int i = 0; i < usage.size(); i++) {
            const UsageEntry& entry = usage[i];
            ss << (entry.optional ? "[" : "");
            ss << entry.name
               << (entry.param.name
                       ? (" " + (entry.param.optional ? ("[" + *entry.param.name + "]") : *entry.param.name))
                       : "");
            ss << (entry.optional ? "]" : "");
            ss << ((i < usage.size() - 1) ? " " : "");
        }

        std::cout << "usage: " << wrap(ss.str(), 7, max_width) << std::endl << std::endl;
    }

    static const std::string pad = "  ";

    // Print positional arguments
    {
        std::cout << "positional arguments:" << std::endl;
        for (const auto& [name, help] : positional_entries) {
            std::stringstream ss;
            ss << pad << std::left << std::setw(static_cast<int>(args_col_width)) << name << help.value_or("");
            std::cout << wrap(ss.str(), args_col_width + pad.size(), max_width) << std::endl;
        }
        std::cout << std::endl;
    }

    // Print options
    {
        std::cout << "options:" << std::endl;
        for (const auto& [names, param_name, help] : option_entries) {
            std::stringstream ss1;
            for (unsigned int i = 0; i < names.size(); i++) {
                ss1 << names[i] << ((i < names.size() - 1) ? ", " : "");
            }
            ss1 << (param_name ? (" " + *param_name) : "");

            std::stringstream ss2;
            ss2 << pad << std::left << std::setw(static_cast<int>(args_col_width)) << ss1.str() << help.value_or("");
            std::cout << wrap(ss2.str(), args_col_width + pad.size(), max_width) << std::endl;
        }
    }
}
} // namespace Args
