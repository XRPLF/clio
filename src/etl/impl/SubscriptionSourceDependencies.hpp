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

#pragma once

#include "feed/SubscriptionManagerInterface.hpp"

#include <boost/json/object.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace etl::impl {

class SubscriptionSourceDependencies {
    struct Concept;
    std::unique_ptr<Concept> pImpl_;

public:
    template <typename NetworkValidatedLedgersType>
    SubscriptionSourceDependencies(
        std::shared_ptr<NetworkValidatedLedgersType> networkValidatedLedgers,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions
    )
        : pImpl_{std::make_unique<Model<NetworkValidatedLedgersType>>(
              std::move(networkValidatedLedgers),
              std::move(subscriptions)
          )}
    {
    }

    void
    forwardProposedTransaction(boost::json::object const& receivedTxJson)
    {
        pImpl_->forwardProposedTransaction(receivedTxJson);
    }

    void
    forwardValidation(boost::json::object const& validationJson) const
    {
        pImpl_->forwardValidation(validationJson);
    }
    void
    forwardManifest(boost::json::object const& manifestJson) const
    {
        pImpl_->forwardManifest(manifestJson);
    }
    void
    pushValidatedLedger(uint32_t const idx)
    {
        pImpl_->pushValidatedLedger(idx);
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void
        forwardProposedTransaction(boost::json::object const& receivedTxJson) = 0;
        virtual void
        forwardValidation(boost::json::object const& validationJson) const = 0;
        virtual void
        forwardManifest(boost::json::object const& manifestJson) const = 0;
        virtual void
        pushValidatedLedger(uint32_t idx) = 0;
    };

    template <typename SomeNetworkValidatedLedgersType>
    class Model : public Concept {
        std::shared_ptr<SomeNetworkValidatedLedgersType> networkValidatedLedgers_;
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

    public:
        Model(
            std::shared_ptr<SomeNetworkValidatedLedgersType> networkValidatedLedgers,
            std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions
        )
            : networkValidatedLedgers_{std::move(networkValidatedLedgers)}, subscriptions_{std::move(subscriptions)}
        {
        }
        void
        forwardProposedTransaction(boost::json::object const& receivedTxJson) override
        {
            subscriptions_->forwardProposedTransaction(receivedTxJson);
        }
        void
        forwardValidation(boost::json::object const& validationJson) const override
        {
            subscriptions_->forwardValidation(validationJson);
        }
        void
        forwardManifest(boost::json::object const& manifestJson) const override
        {
            subscriptions_->forwardManifest(manifestJson);
        }
        void
        pushValidatedLedger(uint32_t idx) override
        {
            networkValidatedLedgers_->push(idx);
        }
    };
};

}  // namespace etl::impl
