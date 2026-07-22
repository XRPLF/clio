#pragma once

namespace tests::util {

static auto const kNameGenerator = [](auto const& info) { return info.param.testName; };

}  // namespace tests::util
