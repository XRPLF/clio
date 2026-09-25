#pragma once

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "etl/ETLServiceInterface.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/tx/Types.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The tx method retrieves information on a single transaction, by its identifying hash.
 *
 * For more details see: https://xrpl.org/tx.html
 */
class TxHandler : public rpc::HandlerFor<rpc::spec::handlers::tx::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<etl::ETLServiceInterface const> etl_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t date = 0u;
        std::string hash = {};  // NOLINT(readability-redundant-member-init)
        uint32_t ledgerIndex = 0u;
        std::optional<boost::json::object> meta =
            std::nullopt;  // NOLINT(readability-redundant-member-init)
        std::optional<boost::json::object> tx =
            std::nullopt;  // NOLINT(readability-redundant-member-init)
        std::optional<std::string> metaStr =
            std::nullopt;  // NOLINT(readability-redundant-member-init)
        std::optional<std::string> txStr =
            std::nullopt;  // NOLINT(readability-redundant-member-init)
        std::optional<std::string> ctid =
            std::nullopt;  // NOLINT(readability-redundant-member-init) ctid when binary=true
        std::optional<xrpl::LedgerHeader> ledgerHeader =
            std::nullopt;  // NOLINT(readability-redundant-member-init) ledger hash when apiVersion
                           // >= 2
        uint32_t apiVersion = 0u;
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new TxHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param etl The ETL service to use
     */
    TxHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<etl::ETLServiceInterface const> const& etl
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), etl_(etl)
    {
    }

    /**
     * @brief Process the Tx command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const
    {
        if (input.ctid && input.transaction)  // ambiguous identifier
            return Error{Status{XrpldError::RpcInvalidParams}};

        if (!input.ctid && !input.transaction)  // at least one identifier must be supplied
            return Error{Status{XrpldError::RpcInvalidParams}};

        static constexpr auto kMaxLedgerRange = 1000u;
        auto const rangeSupplied = input.minLedger && input.maxLedger;

        if (rangeSupplied) {
            if (*input.minLedger > *input.maxLedger)
                return Error{Status{XrpldError::RpcInvalidLgrRange}};

            if (*input.maxLedger - *input.minLedger > kMaxLedgerRange)
                return Error{Status{XrpldError::RpcExcessiveLgrRange}};
        }

        std::optional<uint32_t> currentNetId = std::nullopt;
        if (auto const& etlState = etl_->getETLState(); etlState.has_value())
            currentNetId = etlState->networkID;

        std::optional<data::TransactionAndMetadata> dbResponse;

        if (input.ctid) {
            auto const ctid = rpc::decodeCTID(*input.ctid);
            if (!ctid)
                return Error{Status{XrpldError::RpcInvalidParams}};

            auto const [lgrSeq, txnIdx, netId] = *ctid;
            // when current network id is available, let us check the network id from parameter
            if (currentNetId && netId != *currentNetId) {
                return Error{Status{
                    XrpldError::RpcWrongNetwork,
                    fmt::format(
                        "Wrong network. You should submit this request to a node running on "
                        "NetworkID: {}",
                        netId
                    )
                }};
            }

            dbResponse = fetchTxViaCtid(lgrSeq, txnIdx, ctx.yield);
        } else {
            dbResponse = sharedPtrBackend_->fetchTransaction(*input.transaction, ctx.yield);
        }

        auto output = TxHandler::Output{.apiVersion = ctx.apiVersion};

        if (!dbResponse) {
            if (rangeSupplied && input.transaction)  // ranges not for ctid
            {
                auto const range = sharedPtrBackend_->fetchLedgerRange();
                ASSERT(range.has_value(), "Tx's ledger range must be available");

                // NOLINTBEGIN(bugprone-unchecked-optional-access)
                auto const searchedAll = range->maxSequence >= *input.maxLedger &&
                    range->minSequence <= *input.minLedger;
                // NOLINTEND(bugprone-unchecked-optional-access)

                return Error{
                    Status{XrpldError::RpcTxnNotFound, ExtraInfo{{"searched_all", searchedAll}}}
                };
            }

            return Error{Status{XrpldError::RpcTxnNotFound}};
        }

        auto const [txn, meta] =
            toExpandedJson(*dbResponse, ctx.apiVersion, NFTokenjson::ENABLE, currentNetId);

        if (!input.binary) {
            output.tx = txn;
            output.meta = meta;
        } else {
            output.txStr = xrpl::strHex(dbResponse->transaction);
            output.metaStr = xrpl::strHex(dbResponse->metadata);

            // input.transaction might be not available, get hash via tx object
            if (txn.contains(JS(hash)))
                output.hash = txn.at(JS(hash)).as_string();
        }

        // append ctid here to mimic rippled behavior
        auto const txnIdx = boost::json::value_to<uint64_t>(meta.at("TransactionIndex"));
        if (txnIdx <= 0xFFFFU && dbResponse->ledgerSequence < 0x0FFF'FFFFUL && currentNetId &&
            *currentNetId <= 0xFFFFU) {
            output.ctid = rpc::encodeCTID(
                dbResponse->ledgerSequence,
                static_cast<uint16_t>(txnIdx),
                static_cast<uint16_t>(*currentNetId)
            );
        }

        output.date = dbResponse->date;
        output.ledgerIndex = dbResponse->ledgerSequence;

        // fetch ledger hash
        if (ctx.apiVersion > 1u) {
            output.ledgerHeader =
                sharedPtrBackend_->fetchLedgerBySequence(dbResponse->ledgerSequence, ctx.yield);
        }

        return output;
    }

private:
    [[nodiscard]] std::optional<data::TransactionAndMetadata>
    fetchTxViaCtid(uint32_t ledgerSeq, uint32_t txId, boost::asio::yield_context yield) const
    {
        auto const txs = sharedPtrBackend_->fetchAllTransactionsInLedger(ledgerSeq, yield);

        for (auto const& tx : txs) {
            auto const [txn, meta] = deserializeTxPlusMeta(tx, ledgerSeq);

            if (meta->getIndex() == txId)
                return tx;
        }

        return std::nullopt;
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        auto const getJsonV1 = [&]() {
            auto obj = boost::json::object{};

            if (output.tx) {
                obj = *output.tx;
                obj[JS(meta)] = *output.meta;
            } else {
                obj[JS(meta)] = *output.metaStr;
                obj[JS(tx)] = *output.txStr;
                obj[JS(hash)] = output.hash;
            }

            obj[JS(validated)] = output.validated;
            obj[JS(date)] = output.date;
            obj[JS(ledger_index)] = output.ledgerIndex;
            obj[JS(inLedger)] = output.ledgerIndex;
            return obj;
        };

        auto const getJsonV2 = [&]() {
            auto obj = boost::json::object{};

            if (output.tx) {
                obj[JS(tx_json)] = *output.tx;
                obj[JS(tx_json)].as_object()[JS(date)] = output.date;
                if (output.ctid)
                    obj[JS(tx_json)].as_object()[JS(ctid)] = *output.ctid;

                obj[JS(tx_json)].as_object()[JS(ledger_index)] = output.ledgerIndex;
                // move hash from tx_json to root
                if (obj[JS(tx_json)].as_object().contains(JS(hash))) {
                    obj[JS(hash)] = obj[JS(tx_json)].as_object()[JS(hash)];
                    obj[JS(tx_json)].as_object().erase(JS(hash));
                }
                obj[JS(meta)] = *output.meta;
            } else {
                obj[JS(meta_blob)] = *output.metaStr;
                obj[JS(tx_blob)] = *output.txStr;
                obj[JS(hash)] = output.hash;
            }

            obj[JS(validated)] = output.validated;
            obj[JS(ledger_index)] = output.ledgerIndex;

            if (output.ledgerHeader) {
                obj[JS(ledger_hash)] = xrpl::strHex(output.ledgerHeader->hash);
                obj[JS(close_time_iso)] = xrpl::toStringIso(output.ledgerHeader->closeTime);
            }
            return obj;
        };

        auto obj = output.apiVersion > 1u ? getJsonV2() : getJsonV1();

        if (output.ctid)
            obj[JS(ctid)] = *output.ctid;

        jv = std::move(obj);
    }
};

}  // namespace rpc
