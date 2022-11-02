#include <util/Taggable.h>

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

TagDecoratorFactory
TagDecoratorFactory::with(parent_t parent) const noexcept
{
    return TagDecoratorFactory(type_, parent);
}

}  // namespace util
