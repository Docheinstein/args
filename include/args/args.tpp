#ifndef ARGS_TPP
#define ARGS_TPP

#include <optional>

namespace Args {
namespace Traits {
    template <typename T, typename U, typename = void>
    struct IsOptionalOf : std::false_type {};

    template <typename T, typename U>
    struct IsOptionalOf<T, U, std::enable_if_t<std::is_same_v<typename T::value_type, U>>> : std::true_type {};

    template <typename T, typename U>
    inline constexpr auto IsOptionalOfV = IsOptionalOf<T, U>::value;

    template <typename T, typename = void>
    struct IsOptionalArithmetic : std::false_type {};

    template <typename T>
    struct IsOptionalArithmetic<T, std::enable_if_t<std::is_arithmetic_v<typename T::value_type>>> : std::true_type {};

    template <typename T>
    inline constexpr auto IsOptionalArithmeticV = IsOptionalArithmetic<T>::value;

    template <typename T, typename = void>
    struct IsOptionalIntegral : std::false_type {};

    template <typename T>
    struct IsOptionalIntegral<T, std::enable_if_t<std::is_integral_v<typename T::value_type>>> : std::true_type {};

    template <typename T>
    inline constexpr auto IsOptionalIntegralV = IsOptionalIntegral<T>::value;

    template <typename T>
    inline constexpr auto IsOptionalV = IsOptionalOfV<T, std::string> || IsOptionalArithmeticV<T>;
} // namespace Traits

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
        return;
    }

    if constexpr (Traits::IsOptionalV<T>) {
        // Optional
        if (!feed.has_next() || (!feed.seek_next().empty() && feed.seek_next()[0] == '-' /* next is option */)) {
            // The optional argument cannot consume the next token: set the default value
            this->data = typename T::value_type {};
            return;
        }
    }

    if constexpr (std::is_same_v<T, std::string> || Traits::IsOptionalOfV<T, std::string>) {
        // String
        this->data = feed.pop_next();
        return;
    }

    if constexpr (std::is_arithmetic_v<T> || Traits::IsOptionalArithmeticV<T>) {
        // Number
        const std::string& next = feed.pop_next();
        const char* cstr = next.c_str();

        char* endptr {};
        errno = 0;

        if constexpr (std::is_integral_v<T> || Traits::IsOptionalIntegralV<T>) {
            this->data = std::strtol(cstr, &endptr, 10);
        } else {
            static_assert(std::is_floating_point_v<T>);
            this->data = std::strtof(cstr, &endptr);
        }

        if (errno || endptr == cstr) {
            feed.add_error("failed to parse '" + next + "' as number");
        }
        return;
    }
}

template <typename T>
unsigned int ArgumentImpl<T>::num_min_params() const {
    if constexpr (std::is_same_v<T, bool> || Traits::IsOptionalV<T>) {
        return 0;
    } else {
        return 1;
    }
}

    template <typename T>
    unsigned int ArgumentImpl<T>::num_max_params() const {
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
