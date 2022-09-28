#include <util/Taggable.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <atomic>
#include <mutex>
#include <string>

namespace util::detail {

UIntTagGenerator::tag_t
UIntTagGenerator::next()
{
    static std::atomic_uint64_t num{0};
    return num++;
}

UUIDTagGenerator::tag_t
UUIDTagGenerator::next()
{
    static boost::uuids::random_generator gen{};
    static std::mutex mtx{};

    std::lock_guard lk(mtx);
    return gen();
}

}  // namespace util::detail

namespace util {

std::unique_ptr<BaseTagDecorator>
TagDecoratorFactory::make() const
{
    switch (type_)
    {
        case Type::UINT:
            return std::make_unique<TagDecorator<detail::UIntTagGenerator>>(
                parent_);
        case Type::UUID:
            return std::make_unique<TagDecorator<detail::UUIDTagGenerator>>(
                parent_);
        case Type::NONE:
        default:
            return std::make_unique<TagDecorator<detail::NullTagGenerator>>();
    }
}

TagDecoratorFactory::Type
TagDecoratorFactory::parseType(boost::json::object const& config)
{
    if (!config.contains("log_tag_style"))
        return TagDecoratorFactory::Type::NONE;

    auto style = config.at("log_tag_style").as_string();
    if (boost::iequals(style, "int") || boost::iequals(style, "uint"))
        return TagDecoratorFactory::Type::UINT;
    else if (boost::iequals(style, "null") || boost::iequals(style, "none"))
        return TagDecoratorFactory::Type::NONE;
    else if (boost::iequals(style, "uuid"))
        return TagDecoratorFactory::Type::UUID;
    else
        throw std::runtime_error(
            "Could not parse `log_tag_style`: expected `uint`, `uuid` or "
            "`null`");
}

TagDecoratorFactory
TagDecoratorFactory::with(parent_t parent) const noexcept
{
    return TagDecoratorFactory(type_, parent);
}

}  // namespace util
