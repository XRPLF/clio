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

#include <util/Taggable.h>

#include <boost/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <atomic>
#include <mutex>
#include <string>

namespace clio::util::detail {

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

}  // namespace clio::util::detail

namespace clio::util {

std::unique_ptr<BaseTagDecorator>
TagDecoratorFactory::make() const
{
    switch (type_)
    {
        case Type::UINT:
            return std::make_unique<TagDecorator<detail::UIntTagGenerator>>(parent_);
        case Type::UUID:
            return std::make_unique<TagDecorator<detail::UUIDTagGenerator>>(parent_);
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

}  // namespace clio::util
