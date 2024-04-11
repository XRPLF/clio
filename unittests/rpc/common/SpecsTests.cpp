//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/Errors.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace rpc;

using testing::Return;
using testing::StrictMock;

struct SpecsTests : testing::Test {
    struct RequirementMock {
        MOCK_METHOD(MaybeError, verify, (boost::json::value const&, std::string));
    };

    struct RequirementMockRef {
        RequirementMockRef(StrictMock<RequirementMock>& ref) : ref(ref)
        {
        }
        MaybeError
        verify(boost::json::value& value, std::string key) const
        {
            return ref.verify(value, key);
        }

        StrictMock<RequirementMock>& ref;
    };

    struct CheckMock {
        MOCK_METHOD(std::optional<check::Warning>, check, (boost::json::value const&, std::string));
    };

    struct CheckMockRef {
        CheckMockRef(StrictMock<CheckMock>& ref) : ref(ref)
        {
        }

        std::optional<check::Warning>
        check(boost::json::value const& value, std::string key) const
        {
            return ref.check(value, key);
        }

        StrictMock<CheckMock>& ref;
    };

    StrictMock<RequirementMock> requirementMock;
    StrictMock<RequirementMock> anotherRequirementMock;
    StrictMock<CheckMock> checkMock;
    StrictMock<CheckMock> anotherCheckMock;
};

struct ProcessorTestBundle {
    std::string name;
    MaybeError requirementResult;
    std::optional<MaybeError> otherRequirementResult;
    MaybeError expectedResult;
};

struct FieldProcessorTests : SpecsTests, testing::WithParamInterface<ProcessorTestBundle> {
    FieldSpec spec_{"key", RequirementMockRef(requirementMock), RequirementMockRef(anotherRequirementMock)};
    boost::json::value json_;
};

INSTANTIATE_TEST_SUITE_P(
    FieldSpecTestGroup,
    FieldProcessorTests,
    testing::Values(
        ProcessorTestBundle{"NoErrors", MaybeError{}, MaybeError{}, MaybeError{}},
        ProcessorTestBundle{
            "FirstError",
            Error{Status{"error1"}},
            std::nullopt,
            Error{Status{"error1"}},
        },
        ProcessorTestBundle{
            "SecondError",
            MaybeError{},
            Error{Status{"error2"}},
            Error{Status{"error2"}},
        }
    ),
    [](testing::TestParamInfo<ProcessorTestBundle> const& info) { return info.param.name; }
);

TEST_P(FieldProcessorTests, FieldSpecWithRequirementProcess)
{
    EXPECT_CALL(requirementMock, verify).WillOnce(testing::Return(GetParam().requirementResult));
    if (GetParam().otherRequirementResult) {
        EXPECT_CALL(anotherRequirementMock, verify)
            .WillOnce(testing::Return(GetParam().otherRequirementResult.value()));
    }

    auto const result = spec_.process(json_);
    EXPECT_EQ(result, GetParam().expectedResult);
}

TEST_F(FieldProcessorTests, FieldSpecWithRequirementCheck)
{
    auto const result = spec_.check(json_);
    EXPECT_EQ(result, check::Warnings{});
}

struct FieldCheckerTestBundle {
    std::string name;
    std::optional<check::Warning> checkResult;
    std::optional<check::Warning> otherCheckResult;
    check::Warnings expectedWarnings;
};

struct FieldCheckerTests : SpecsTests, testing::WithParamInterface<FieldCheckerTestBundle> {
    FieldSpec spec_{"key", CheckMockRef(checkMock), CheckMockRef(anotherCheckMock)};
    boost::json::value json_;
};

INSTANTIATE_TEST_SUITE_P(
    FieldSpecTestGroup,
    FieldCheckerTests,
    testing::Values(
        FieldCheckerTestBundle{"NoWarnings", std::nullopt, std::nullopt, check::Warnings{}},
        FieldCheckerTestBundle{
            "FirstWarning",
            check::Warning{WarningCode::warnUNKNOWN, "error1"},
            std::nullopt,
            check::Warnings{check::Warning{WarningCode::warnUNKNOWN, "error1"}}
        },
        FieldCheckerTestBundle{
            "SecondWarning",
            std::nullopt,
            check::Warning{WarningCode::warnUNKNOWN, "error2"},
            check::Warnings{check::Warning{WarningCode::warnUNKNOWN, "error2"}}
        },
        FieldCheckerTestBundle{
            "BothWarnings",
            check::Warning{WarningCode::warnUNKNOWN, "error1"},
            check::Warning{WarningCode::warnUNKNOWN, "error2"},
            check::Warnings{
                check::Warning{WarningCode::warnUNKNOWN, "error1"},
                check::Warning{WarningCode::warnUNKNOWN, "error2"}
            }
        }
    ),
    [](testing::TestParamInfo<FieldCheckerTestBundle> const& info) { return info.param.name; }
);

TEST_F(FieldCheckerTests, FieldSpecWithCheckProcess)
{
    auto const result = spec_.process(json_);
    EXPECT_EQ(result, MaybeError{});
}

TEST_P(FieldCheckerTests, FieldSpecWithCheck)
{
    EXPECT_CALL(checkMock, check).WillOnce(testing::Return(GetParam().checkResult));
    EXPECT_CALL(anotherCheckMock, check).WillOnce(testing::Return(GetParam().otherCheckResult));
    auto const result = spec_.check(json_);
    EXPECT_EQ(result, GetParam().expectedWarnings);
}

struct RpcSpecProcessTests : SpecsTests, testing::WithParamInterface<ProcessorTestBundle> {
    RpcSpec spec{{"key1", RequirementMockRef(requirementMock)}, {"key2", RequirementMockRef(anotherRequirementMock)}};
    boost::json::value json;
};

INSTANTIATE_TEST_SUITE_P(
    RpcSpecProcessTestGroup,
    RpcSpecProcessTests,
    testing::Values(
        ProcessorTestBundle{"NoErrors", MaybeError{}, MaybeError{}, MaybeError{}},
        ProcessorTestBundle{
            "FirstError",
            Error{Status{"error1"}},
            std::nullopt,
            Error{Status{"error1"}},
        },
        ProcessorTestBundle{
            "SecondError",
            MaybeError{},
            Error{Status{"error2"}},
            Error{Status{"error2"}},
        }
    ),
    [](testing::TestParamInfo<ProcessorTestBundle> const& info) { return info.param.name; }
);

TEST_P(RpcSpecProcessTests, Process)
{
    EXPECT_CALL(requirementMock, verify).WillOnce(testing::Return(GetParam().requirementResult));
    if (GetParam().otherRequirementResult) {
        EXPECT_CALL(anotherRequirementMock, verify)
            .WillOnce(testing::Return(GetParam().otherRequirementResult.value()));
    }
    auto const result = spec.process(json);
    EXPECT_EQ(result, GetParam().expectedResult);
}

struct RpcSpecCheckTestBundle {
    std::string name;
    std::optional<check::Warning> checkResult;
    std::optional<check::Warning> otherCheckResult;
    std::unordered_map<int, std::vector<std::string>> expectedWarnings;
};

struct RpcSpecCheckTests : SpecsTests, testing::WithParamInterface<RpcSpecCheckTestBundle> {
    RpcSpec spec_{{"key1", CheckMockRef(checkMock)}, {"key2", CheckMockRef(anotherCheckMock)}};
    boost::json::value json_;
};

INSTANTIATE_TEST_SUITE_P(
    RpcSpecCheckTestGroup,
    RpcSpecCheckTests,
    testing::Values(
        RpcSpecCheckTestBundle{"NoWarnings", std::nullopt, std::nullopt, {}},
        RpcSpecCheckTestBundle{
            "FirstWarning",
            check::Warning{WarningCode::warnUNKNOWN, "error1"},
            std::nullopt,
            {{WarningCode::warnUNKNOWN, {"error1"}}}
        },
        RpcSpecCheckTestBundle{
            "SecondWarning",
            std::nullopt,
            check::Warning{WarningCode::warnUNKNOWN, "error2"},
            {{WarningCode::warnUNKNOWN, {"error2"}}}
        },
        RpcSpecCheckTestBundle{
            "BothWarnings",
            check::Warning{WarningCode::warnUNKNOWN, "error1"},
            check::Warning{WarningCode::warnUNKNOWN, "error2"},
            {{WarningCode::warnUNKNOWN, {"error1", "error2"}}}
        },
        RpcSpecCheckTestBundle{
            "DifferentWarningCodes",
            check::Warning{WarningCode::warnUNKNOWN, "error1"},
            check::Warning{WarningCode::warnRPC_CLIO, "error2"},
            {{WarningCode::warnUNKNOWN, {"error1"}}, {WarningCode::warnRPC_CLIO, {"error2"}}}
        }

    ),
    [](testing::TestParamInfo<RpcSpecCheckTestBundle> const& info) { return info.param.name; }
);

TEST_P(RpcSpecCheckTests, Check)
{
    EXPECT_CALL(checkMock, check).WillOnce(testing::Return(GetParam().checkResult));
    EXPECT_CALL(anotherCheckMock, check).WillOnce(testing::Return(GetParam().otherCheckResult));

    auto const result = spec_.check(json_);
    ASSERT_EQ(result.size(), GetParam().expectedWarnings.size());
    for (auto const& entry : result) {
        ASSERT_TRUE(entry.is_object());
        auto const& object = entry.as_object();
        ASSERT_TRUE(object.contains("id"));
        ASSERT_TRUE(object.at("id").is_int64());
        ASSERT_TRUE(object.contains("message"));
        ASSERT_TRUE(object.at("message").is_string());
        auto it = GetParam().expectedWarnings.find(object.at("id").as_int64());
        ASSERT_NE(it, GetParam().expectedWarnings.end());
        for (auto const& message : it->second) {
            EXPECT_NE(object.at("message").as_string().find(message), std::string::npos);
        }
    }
}
