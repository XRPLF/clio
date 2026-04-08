#include "util/MockAssert.hpp"

#include "util/Assert.hpp"

#include <string>
#include <string_view>

namespace common::util {

WithMockAssert::WithMockAssert()
{
    ::util::impl::OnAssert::setAction([](std::string_view m) { WithMockAssert::throwOnAssert(m); });
}

WithMockAssert::~WithMockAssert()
{
    ::util::impl::OnAssert::resetAction();
}

void
WithMockAssert::throwOnAssert(std::string_view m)
{
    throw MockAssertException{.message = std::string{m}};
}

WithMockAssertNoThrow::~WithMockAssertNoThrow()
{
    ::util::impl::OnAssert::resetAction();
}

}  // namespace common::util
