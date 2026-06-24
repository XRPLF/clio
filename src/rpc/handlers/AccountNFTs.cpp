#include "rpc/handlers/AccountNFTs.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nft.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc {

AccountNFTsHandler::Result
AccountNFTsHandler::process(AccountNFTsHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountNFT's ledger range must be available");
    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        xrpl::keylet::account(*accountID).key,
        lgrInfo.seq,
        ctx.yield
    );

    if (!accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

    auto response = Output{};
    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    // if a marker was passed, start at the page specified in marker. Else, start at the max page
    auto const pageKey = input.marker ? xrpl::uint256{input.marker->c_str()}
                                      // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                      : xrpl::keylet::nftpageMax(*accountID).key;
    auto const blob = sharedPtrBackend_->fetchLedgerObject(pageKey, lgrInfo.seq, ctx.yield);

    if (!blob) {
        if (input.marker.has_value()) {
            return Error{Status{
                RippledError::RpcInvalidParams, "Marker field does not match any valid Page ID"
            }};
        }
        return response;
    }

    std::optional<xrpl::SLE const> page{
        xrpl::SLE{xrpl::SerialIter{blob->data(), blob->size()}, pageKey}
    };

    if (page->getType() != xrpl::ltNFTOKEN_PAGE) {
        return Error{
            Status{RippledError::RpcInvalidParams, "Marker matches Page ID from another Account"}
        };
    }

    auto numPages = 0u;

    while (page) {
        auto const arr = page->getFieldArray(xrpl::sfNFTokens);

        for (auto const& nft : arr) {
            auto const nftokenID = nft[xrpl::sfNFTokenID];

            response.nfts.push_back(toBoostJson(nft.getJson(xrpl::JsonOptions::Values::None)));
            auto& obj = response.nfts.back().as_object();

            // Pull out the components of the nft ID.
            obj[SFS(sfFlags)] = xrpl::nft::getFlags(nftokenID);
            obj[SFS(sfIssuer)] = to_string(xrpl::nft::getIssuer(nftokenID));
            obj[SFS(sfNFTokenTaxon)] = xrpl::nft::toUInt32(xrpl::nft::getTaxon(nftokenID));
            obj[JS(nft_serial)] = xrpl::nft::getSerial(nftokenID);

            if (std::uint16_t const xferFee = {xrpl::nft::getTransferFee(nftokenID)})
                obj[SFS(sfTransferFee)] = xferFee;
        }

        ++numPages;
        if (auto const npm = (*page)[~xrpl::sfPreviousPageMin]) {
            auto const nextKey = xrpl::Keylet(xrpl::ltNFTOKEN_PAGE, *npm);
            if (numPages == input.limit) {
                response.marker = to_string(nextKey.key);
                return response;
            }

            auto const nextBlob =
                sharedPtrBackend_->fetchLedgerObject(nextKey.key, lgrInfo.seq, ctx.yield);
            page.emplace(
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                xrpl::SLE{xrpl::SerialIter{nextBlob->data(), nextBlob->size()}, nextKey.key}
            );
        } else {
            page.reset();
        }
    }

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountNFTsHandler::Output const& output
)
{
    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(account), output.account},
        {JS(account_nfts), output.nfts},
        {JS(limit), output.limit},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = *output.marker;
}

AccountNFTsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountNFTsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountNFTsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jsonObject.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jsonObject.at(JS(limit)));

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jsonObject.at(JS(marker)));

    return input;
}

}  // namespace rpc
