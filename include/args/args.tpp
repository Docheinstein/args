#ifndef ARGS_TPP
#define ARGS_TPP

#include <optional>

namespace Args {
template <typename T>
ArgumentImplT<T>::ArgumentImplT(T& data, std::vector<std::string>&& names) :
    Argument {std::move(names)},
    data {data} {
}

template <typename T>
ArgumentImpl<T>::ArgumentImpl(T& data, std::vector<std::string> names) :
    ArgumentImplT<T> {data, std::move(names)} {
}

template <typename T>
void ArgumentImpl<T>::parse(ArgumentParseContext& feed) {
    if constexpr (std::is_same_v<T, bool>) {
        // Boolean
        this->data = true;
    } else if constexpr (std::is_same_v<T, std::string>) {
        // String
        this->data = feed.pop_next();
    } else if constexpr (std::is_arithmetic_v<T>) {
        // Numbers
        const std::string& next = feed.pop_next();
        const char* cstr = next.c_str();

        char* endptr {};
        errno = 0;

        if constexpr (std::is_integral_v<T>) {
            this->data = std::strtol(cstr, &endptr, 10);
        } else {
            static_assert(std::is_floating_point_v<T>);
            this->data = std::strtof(cstr, &endptr);
        }

        if (errno || endptr == cstr) {
            feed.add_error("failed to parse '" + next + "' as number");
        }
    }
}

template <typename T>
unsigned int ArgumentImpl<T>::num_params() const {
    if constexpr (std::is_same_v<T, bool>) {
        return 0;
    } else {
        return 1;
    }
}

template <typename T, typename Name, typename... OtherNames>
ArgumentConfig& Parser::add_argument(T& data, Name primary_name, OtherNames... alternative_names) {
    // Build the names
    std::vector<std::string> names {primary_name, alternative_names...};

    // Figure out if argument is positional or an option
    std::optional<bool> is_option {};
    for (const auto& name : names) {
        if (name.empty()) {
            setup_errors.emplace_back("empty argument name");
            continue;
        }

        bool current_is_option = name[0] == '-';

        if (!is_option) {
            // First name
            is_option = current_is_option;
        } else {
            // Alternative names
            if (*is_option != current_is_option) {
                setup_errors.emplace_back("all argument's names must either be optional or positional");
            }
        }
    }

    // Build the argument
    arguments.push_back(std::make_unique<ArgumentImpl<T>>(data, names));
    std::unique_ptr<Argument>& arg = arguments.back();

    if (*is_option) {
        // Option
        for (const auto& name : names) {
            options.emplace(name, &*arg);
        }
    } else {
        // Positional argument
        arg->required_ = true;
        positionals.push_back(&*arg);
    }

    return *arg;
}
} // namespace Args

#endif // ARGS_TPP
