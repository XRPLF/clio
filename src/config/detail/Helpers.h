#pragma once

#include <optional>
#include <queue>
#include <stdexcept>
#include <string>

namespace clio::detail {

/**
 * @brief Thrown when a KeyPath related error occurs
 */
struct KeyException : public ::std::logic_error
{
    KeyException(::std::string msg) : ::std::logic_error{msg}
    {
    }
};

/**
 * @brief Thrown when a Store (config's storage) related error occurs.
 */
struct StoreException : public ::std::logic_error
{
    StoreException(::std::string msg) : ::std::logic_error{msg}
    {
    }
};

/**
 * @brief Simple string tokenizer. Used by @ref Config.
 *
 * @tparam KeyType The type of key to use
 * @tparam Separator The separator character
 */
template <typename KeyType, char Separator>
class Tokenizer final
{
    using opt_key_t = std::optional<KeyType>;
    KeyType key_;
    KeyType token_{};
    std::queue<KeyType> tokens_{};

public:
    explicit Tokenizer(KeyType key) : key_{key}
    {
        if (key.empty())
            throw KeyException("Empty key");

        for (auto const& c : key)
        {
            if (c == Separator)
                saveToken();
            else
                token_ += c;
        }

        saveToken();
    }

    [[nodiscard]] opt_key_t
    next()
    {
        if (tokens_.empty())
            return std::nullopt;
        auto token = tokens_.front();
        tokens_.pop();
        return std::make_optional(std::move(token));
    }

private:
    void
    saveToken()
    {
        if (token_.empty())
            throw KeyException("Empty token in key '" + key_ + "'.");
        tokens_.push(std::move(token_));
        token_ = {};
    }
};

template <typename T>
static constexpr const char*
typeName()
{
    return typeid(T).name();
}

template <>
constexpr const char*
typeName<uint64_t>()
{
    return "uint64_t";
}

template <>
constexpr const char*
typeName<int64_t>()
{
    return "int64_t";
}

template <>
constexpr const char*
typeName<uint32_t>()
{
    return "uint32_t";
}

template <>
constexpr const char*
typeName<int32_t>()
{
    return "int32_t";
}

template <>
constexpr const char*
typeName<bool>()
{
    return "bool";
}

template <>
constexpr const char*
typeName<std::string>()
{
    return "std::string";
}

template <>
constexpr const char*
typeName<const char*>()
{
    return "const char*";
}

template <>
constexpr const char*
typeName<double>()
{
    return "double";
}

};  // namespace clio::detail
