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

#include "etl/impl/GrpcSource.h"
#include "util/Fixtures.h"
#include "util/MockXrpLedgerAPIService.h"

#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

using namespace etl::impl;

struct GrpcSourceTests : NoLoggerFixture, unittests::util::WithMockXrpLedgerAPIService {
    GrpcSourceTests() : WithMockXrpLedgerAPIService("localhost:50051"), grpcSource("127.0.0.1", "50051", nullptr)
    {
    }

    GrpcSource grpcSource;
};

TEST_F(GrpcSourceTests, fetchLedger)
{
    EXPECT_CALL(mockXrpLedgerAPIService, GetLedger);
    grpcSource.fetchLedger(1, true, false);
}
