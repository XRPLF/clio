#include "util/Taggable.hpp"

#include <boost/uuid/random_generator.hpp>

#include <atomic>
#include <memory>
#include <mutex>

namespace util::impl {

UIntTagGenerator::TagType
UIntTagGenerator::next()
{
    static std::atomic_uint64_t kNum{0};
    return kNum++;
}

UUIDTagGenerator::TagType
UUIDTagGenerator::next()
{
    static boost::uuids::random_generator kGen{};
    static std::mutex kMtx{};

    std::scoped_lock const lk(kMtx);
    return kGen();
}

}  // namespace util::impl

namespace util {

std::unique_ptr<BaseTagDecorator>
TagDecoratorFactory::make() const
{
    switch (type_) {
        case Type::UINT:
            return std::make_unique<TagDecorator<impl::UIntTagGenerator>>(parent_);
        case Type::UUID:
            return std::make_unique<TagDecorator<impl::UUIDTagGenerator>>(parent_);
        case Type::NONE:
        default:
            return std::make_unique<TagDecorator<impl::NullTagGenerator>>();
    }
}

TagDecoratorFactory
TagDecoratorFactory::with(ParentType parent) const noexcept
{
    return TagDecoratorFactory(type_, parent);
}

}  // namespace util
