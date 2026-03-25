#pragma once

namespace tests::util {

static auto const kNAME_GENERATOR = [](auto const& info) { return info.param.testName; };

}  // namespace tests::util
