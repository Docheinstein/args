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

    ArgumentConfig& help(const std::string& h);

protected:
    std::vector<std::string> names {};
    std::string help_ {};
};

class ArgumentParseFeed {
public:
    ArgumentParseFeed(const std::vector<std::string>& argv, std::vector<std::string>& errors, unsigned int index = 0);

    [[nodiscard]] bool hasNext(unsigned int n = 1) const;
    [[nodiscard]] const std::string& seekNext() const;
    const std::string& popNext();
    void addError(std::string&& error) const;

private:
    const std::vector<std::string>& argv;
    std::vector<std::string>& errors;
    unsigned int index {};
};

class IParsableArgument {
public:
    virtual ~IParsableArgument() = default;

    virtual void parse(ArgumentParseFeed& feed) = 0;

    [[nodiscard]] virtual unsigned int numParams() const = 0;
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

    void parse(ArgumentParseFeed& feed) override;

    [[nodiscard]] unsigned int numParams() const override;
};

class Parser {
public:
    Parser();

    template <typename T, typename Name, typename... OtherNames>
    ArgumentConfig& addArgument(T& data, Name primaryName, OtherNames... alternativeNames);

    bool parse(unsigned int argc, char** argv, unsigned int from = 0);

private:
    void printHelp() const;

    std::vector<std::unique_ptr<Argument>> arguments {};
    std::vector<Argument*> positionals {};
    std::unordered_map<std::string, Argument*> optionals {};

    std::vector<std::string> setupErrors {};
    std::vector<std::string> parseErrors {};

    bool helpRequest {};
};
} // namespace Args

#include "args.tpp"

#endif // ARGS_H
