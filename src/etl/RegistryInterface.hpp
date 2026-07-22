#pragma once

#include "etl/Models.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace etl {

/**
 * @brief The interface for a registry that can dispatch transactions and objects to extensions.
 *
 * This class defines the interface for dispatching data through to extensions.
 *
 * @note
 * The registry itself consists of Extensions.
 * Each extension must define at least one valid hook:
 * - for ongoing ETL dispatch:
 *   - void onLedgerData(etl::model::LedgerData const&)
 *   - void onTransaction(uint32_t, etl::model::Transaction const&)
 *   - void onObject(uint32_t, etl::model::Object const&)
 * - for initial ledger load
 *   - void onInitialData(etl::model::LedgerData const&)
 *   - void onInitialTransaction(uint32_t, etl::model::Transaction const&)
 * - for initial objects (called for each downloaded batch)
 *   - void onInitialObjects(uint32_t, std::vector<etl::model::Object> const&, std::string)
 *   - void onInitialObject(uint32_t, etl::model::Object const&)
 *
 * When the registry dispatches (initial)data or objects, each of the above hooks will be called in
 * order on each registered extension. This means that the order of execution is from left to right
 * (hooks) and top to bottom (registered extensions).
 *
 * If either `onTransaction` or `onInitialTransaction` are defined, the extension will have to
 * additionally define a Specification. The specification lists transaction types to filter from the
 * incoming data such that `onTransaction` and `onInitialTransaction` are only called for the
 * transactions that are of interest for the given extension.
 *
 * The specification is setup like so:
 * @code{.cpp}
 * struct Ext {
 *   using spec = etl::model::Spec<
 *     xrpl::TxType::ttNFTOKEN_BURN,
 *     xrpl::TxType::ttNFTOKEN_ACCEPT_OFFER,
 *     xrpl::TxType::ttNFTOKEN_CREATE_OFFER,
 *     xrpl::TxType::ttNFTOKEN_CANCEL_OFFER,
 *     xrpl::TxType::ttNFTOKEN_MINT>;
 *
 *   static void
 *   onInitialTransaction(uint32_t, etl::model::Transaction const&);
 * };
 * @endcode
 */
struct RegistryInterface {
    virtual ~RegistryInterface() = default;

    /**
     * @brief Dispatch initial objects.
     *
     * These objects are received during initial ledger load.
     *
     * @param seq The sequence
     * @param data The objects to dispatch
     * @param lastKey The predcessor of the first object in data if known; an empty string otherwise
     */
    virtual void
    dispatchInitialObjects(
        uint32_t seq,
        std::vector<model::Object> const& data,
        std::string lastKey
    ) = 0;

    /**
     * @brief Dispatch initial ledger data.
     *
     * The transactions, header and edge keys are received during initial ledger load.
     *
     * @param data The data to dispatch
     */
    virtual void
    dispatchInitialData(model::LedgerData const& data) = 0;

    /**
     * @brief Dispatch an entire ledger diff.
     *
     * This is used to dispatch incoming diffs through the extensions.
     *
     * @param data The data to dispatch
     */
    virtual void
    dispatch(model::LedgerData const& data) = 0;
};

}  // namespace etl
