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
void ArgumentImpl<T>::parse(ArgumentParseFeed& feed) {
    if constexpr (std::is_same_v<T, bool>) {
        // Boolean
        this->data = true;
    } else if constexpr (std::is_same_v<T, std::string>) {
        // String
        this->data = feed.popNext();
    } else if constexpr (std::is_arithmetic_v<T>) {
        // Numbers
        const std::string& next = feed.popNext();
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
            feed.addError("failed to parse '" + next + "' as number");
        }
    }
}

template <typename T>
unsigned int ArgumentImpl<T>::numParams() const {
    if constexpr (std::is_same_v<T, bool>) {
        return 0;
    } else {
        return 1;
    }
}

template <typename T, typename Name, typename... OtherNames>
ArgumentConfig& Parser::addArgument(T& data, Name primaryName, OtherNames... alternativeNames) {
    // Build the names
    std::vector<std::string> names {primaryName, alternativeNames...};

    // Build the argument
    arguments.push_back(std::make_unique<ArgumentImpl<T>>(data, names));
    std::unique_ptr<Argument>& arg = arguments.back();

    // Figure out if argument is positional (mandatory) or optional
    std::optional<bool> isOptional {};
    for (const auto& name : names) {
        if (name.empty()) {
            setupErrors.emplace_back("empty argument name");
            continue;
        }

        bool isNameOptional = name[0] == '-';

        if (!isOptional) {
            // First name
            isOptional = isNameOptional;
        } else {
            // Alternative names
            if (*isOptional != isNameOptional) {
                setupErrors.emplace_back("all argument's names must either be optional or positional");
            }
        }
    }

    if (*isOptional) {
        // Optional argument
        for (const auto& name : names) {
            optionals.emplace(name, &*arg);
        }
    } else {
        // Positional argument
        positionals.push_back(&*arg);
    }

    return *arg;
}
} // namespace Args

#endif // ARGS_TPP
