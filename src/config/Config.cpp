#include <config/Config.h>

#include <boost/log/trivial.hpp>
#include <fstream>

namespace clio {

// Note: `store_(store)` MUST use `()` instead of `{}` otherwise gcc
// picks `initializer_list` constructor and anything passed becomes an
// array :-D
Config::Config(boost::json::value store) : store_(std::move(store))
{
}

Config::operator bool() const noexcept
{
    return not store_.is_null();
}

bool
Config::contains(key_type key) const
{
    return lookup(key).has_value();
}

std::optional<boost::json::value>
Config::lookup(key_type key) const
{
    if (store_.is_null())
        return std::nullopt;

    std::reference_wrapper<boost::json::value const> cur = std::cref(store_);
    auto hasBrokenPath = false;
    auto tokenized = detail::Tokenizer<key_type, Separator>{key};
    std::string subkey{};

    auto maybeSection = tokenized.next();
    while (maybeSection.has_value())
    {
        auto section = maybeSection.value();
        subkey += section;

        if (not hasBrokenPath)
        {
            if (not cur.get().is_object())
                throw detail::StoreException(
                    "Not an object at '" + subkey + "'");
            if (not cur.get().as_object().contains(section))
                hasBrokenPath = true;
            else
                cur = std::cref(cur.get().as_object().at(section));
        }

        subkey += Separator;
        maybeSection = tokenized.next();
    }

    if (hasBrokenPath)
        return std::nullopt;
    return std::make_optional(cur);
}

std::optional<Config::array_type>
Config::maybeArray(key_type key) const
{
    try
    {
        auto maybe_arr = lookup(key);
        if (maybe_arr && maybe_arr->is_array())
        {
            auto& arr = maybe_arr->as_array();
            array_type out;
            out.reserve(arr.size());

            std::transform(
                std::begin(arr),
                std::end(arr),
                std::back_inserter(out),
                [](auto&& element) { return Config{std::move(element)}; });
            return std::make_optional<array_type>(std::move(out));
        }
    }
    catch (detail::StoreException const&)
    {
        // ignore store error, but rethrow key errors
    }

    return std::nullopt;
}

Config::array_type
Config::array(key_type key) const
{
    if (auto maybe_arr = maybeArray(key); maybe_arr)
        return maybe_arr.value();
    throw std::logic_error("No array found at '" + key + "'");
}

Config::array_type
Config::arrayOr(key_type key, array_type fallback) const
{
    if (auto maybe_arr = maybeArray(key); maybe_arr)
        return maybe_arr.value();
    return fallback;
}

Config::array_type
Config::arrayOrThrow(key_type key, std::string_view err) const
{
    try
    {
        return maybeArray(key).value();
    }
    catch (std::exception const&)
    {
        throw std::runtime_error(err.data());
    }
}

Config
Config::section(key_type key) const
{
    auto maybe_element = lookup(key);
    if (maybe_element && maybe_element->is_object())
        return Config{std::move(*maybe_element)};
    throw std::logic_error("No section found at '" + key + "'");
}

Config::array_type
Config::array() const
{
    if (not store_.is_array())
        throw std::logic_error("_self_ is not an array");

    array_type out;
    auto const& arr = store_.as_array();
    out.reserve(arr.size());

    std::transform(
        std::cbegin(arr),
        std::cend(arr),
        std::back_inserter(out),
        [](auto const& element) { return Config{element}; });
    return out;
}

Config
ConfigReader::open(std::filesystem::path path)
{
    try
    {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (in)
        {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            return Config{boost::json::parse(contents.str(), {}, opts)};
        }
    }
    catch (std::exception const& e)
    {
        BOOST_LOG_TRIVIAL(error) << "Could not read configuration file from '"
                                 << path.string() << "': " << e.what();
    }

    BOOST_LOG_TRIVIAL(warning) << "Using empty default configuration";
    return Config{};
}

}  // namespace clio
