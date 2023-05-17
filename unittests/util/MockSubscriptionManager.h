//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include <ripple/ledger/ReadView.h>
#include <webserver2/interface/ConnectionBase.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <gmock/gmock.h>

#include <optional>

class MockSubscriptionManager
{
public:
    using session_ptr = std::shared_ptr<Server::ConnectionBase>;
    MockSubscriptionManager()
    {
    }

    MOCK_METHOD(boost::json::object, subLedger, (boost::asio::yield_context&, session_ptr), ());

    MOCK_METHOD(
        void,
        pubLedger,
        (ripple::LedgerInfo const&, ripple::Fees const&, std::string const&, std::uint32_t),
        ());

    MOCK_METHOD(
        void,
        pubBookChanges,
        (ripple::LedgerInfo const&, std::vector<Backend::TransactionAndMetadata> const&),
        ());

    MOCK_METHOD(void, unsubLedger, (session_ptr), ());

    MOCK_METHOD(void, subTransactions, (session_ptr), ());

    MOCK_METHOD(void, unsubTransactions, (session_ptr), ());

    MOCK_METHOD(void, pubTransaction, (Backend::TransactionAndMetadata const&, ripple::LedgerInfo const&), ());

    MOCK_METHOD(void, subAccount, (ripple::AccountID const&, session_ptr&), ());

    MOCK_METHOD(void, unsubAccount, (ripple::AccountID const&, session_ptr const&), ());

    MOCK_METHOD(void, subBook, (ripple::Book const&, session_ptr), ());

    MOCK_METHOD(void, unsubBook, (ripple::Book const&, session_ptr), ());

    MOCK_METHOD(void, subBookChanges, (session_ptr), ());

    MOCK_METHOD(void, unsubBookChanges, (session_ptr), ());

    MOCK_METHOD(void, subManifest, (session_ptr), ());

    MOCK_METHOD(void, unsubManifest, (session_ptr), ());

    MOCK_METHOD(void, subValidation, (session_ptr), ());

    MOCK_METHOD(void, unsubValidation, (session_ptr), ());

    MOCK_METHOD(void, forwardProposedTransaction, (boost::json::object const&), ());

    MOCK_METHOD(void, forwardManifest, (boost::json::object const&), ());

    MOCK_METHOD(void, forwardValidation, (boost::json::object const&), ());

    MOCK_METHOD(void, subProposedAccount, (ripple::AccountID const&, session_ptr), ());

    MOCK_METHOD(void, unsubProposedAccount, (ripple::AccountID const&, session_ptr), ());

    MOCK_METHOD(void, subProposedTransactions, (session_ptr), ());

    MOCK_METHOD(void, unsubProposedTransactions, (session_ptr), ());

    MOCK_METHOD(void, cleanup, (session_ptr), ());

    MOCK_METHOD(boost::json::object, report, (), (const));
};
