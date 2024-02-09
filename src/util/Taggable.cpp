//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/Taggable.hpp"

#include <boost/uuid/random_generator.hpp>

#include <atomic>
#include <memory>
#include <mutex>

namespace util::impl {

UIntTagGenerator::TagType
UIntTagGenerator::next()
{
    static std::atomic_uint64_t num{0};
    return num++;
}

UUIDTagGenerator::TagType
UUIDTagGenerator::next()
{
    static boost::uuids::random_generator gen{};
    static std::mutex mtx{};

    std::lock_guard const lk(mtx);
    return gen();
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
