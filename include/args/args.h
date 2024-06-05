#ifndef ARGS_H
#define ARGS_H

#include <iomanip>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Args {
class ArgumentConfig {
public:
    friend class Parser;

    explicit ArgumentConfig(std::vector<std::string>&& names);

    ArgumentConfig& required(bool req);
    ArgumentConfig& help(const std::string& h);

protected:
    std::vector<std::string> names {};
    std::string help_ {};
    bool required_ {};
};

class ArgumentParseContext {
public:
    ArgumentParseContext(const std::vector<std::string>& argv, std::vector<std::string>& errors,
                         unsigned int index = 0);

    bool has_next(unsigned int n = 1) const;
    const std::string& seek_next() const;
    const std::string& pop_next();
    void add_error(std::string&& error) const;

private:
    const std::vector<std::string>& argv;
    std::vector<std::string>& errors;
    unsigned int index {};
};

class IParsableArgument {
public:
    virtual ~IParsableArgument() = default;

    virtual void parse(ArgumentParseContext& context) = 0;

    virtual unsigned int num_params() const = 0;
};

class Argument : public IParsableArgument, public ArgumentConfig {
public:
    explicit Argument(std::vector<std::string>&& names);
};

template <typename T>
class ArgumentImplT : public Argument {
public:
    ArgumentImplT(T& data, std::vector<std::string>&& names);

protected:
    T& data {};
};

template <typename T>
class ArgumentImpl : public ArgumentImplT<T> {
public:
    ArgumentImpl(T& data, std::vector<std::string> names);

    void parse(ArgumentParseContext& context) override;

    unsigned int num_params() const override;
};

class Parser {
public:
    Parser();

    template <typename T, typename Name, typename... OtherNames>
    ArgumentConfig& add_argument(T& data, Name primary_name, OtherNames... alternative_names);

    bool parse(unsigned int argc, char** argv, unsigned int from = 0);

private:
    void print_help() const;

    std::vector<std::unique_ptr<Argument>> arguments {};
    std::vector<Argument*> positionals {};
    std::unordered_map<std::string, Argument*> options {};

    std::vector<std::string> setup_errors {};
    std::vector<std::string> parse_errors {};

    bool help_request {};
};
} // namespace Args

#include "args.tpp"

#endif // ARGS_H
