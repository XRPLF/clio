#include "rpc/handlers/NFTOffersCommon.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
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

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace xrpl;
using namespace ::rpc;

namespace xrpl {

// TODO: move to some common serialization impl place
inline static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, SLE const& offer)
{
    auto amount = ::toBoostJson(offer.getFieldAmount(sfAmount).getJson(JsonOptions::Values::None));

    boost::json::object obj = {
        {JS(nft_offer_index), to_string(offer.key())},
        {JS(flags), offer[sfFlags]},
        {JS(owner), toBase58(offer.getAccountID(sfOwner))},
        {JS(amount), std::move(amount)},
    };

    if (offer.isFieldPresent(sfDestination))
        obj.insert_or_assign(JS(destination), toBase58(offer.getAccountID(sfDestination)));

    if (offer.isFieldPresent(sfExpiration))
        obj.insert_or_assign(JS(expiration), offer.getFieldU32(sfExpiration));

    jv = std::move(obj);
}

}  // namespace xrpl

namespace rpc {

NFTOffersHandlerBase::Result
NFTOffersHandlerBase::iterateOfferDirectory(
    Input input,
    xrpl::uint256 const& tokenID,
    xrpl::Keylet const& directory,
    boost::asio::yield_context yield
) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "NFTOffersCommon's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    // TODO: just check for existence without pulling
    if (not sharedPtrBackend_->fetchLedgerObject(directory.key, lgrInfo.seq, yield))
        return Error{Status{XrpldError::RpcObjectNotFound, "notFound"}};

    auto output =
        Output{.nftID = xrpl::strHex(input.nftID), .offers = {}, .limit = {}, .marker = {}};
    auto offers = std::vector<xrpl::SLE>{};
    auto reserve = input.limit;
    auto cursor = uint256{};
    auto startHint = uint64_t{0ul};

    if (input.marker) {
        cursor = *input.marker;

        // We have a start point. Use limit - 1 from the result and use the very last one for the
        // resume.
        auto const sle = [this, &cursor, &lgrInfo, yield]() -> std::shared_ptr<SLE const> {
            auto const key = keylet::nftokenOffer(cursor).key;

            if (auto const blob = sharedPtrBackend_->fetchLedgerObject(key, lgrInfo.seq, yield);
                blob)
                return std::make_shared<SLE const>(SerialIter{blob->data(), blob->size()}, key);

            return nullptr;
        }();

        if (!sle || sle->getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltNFTOKEN_OFFER ||
            tokenID != sle->getFieldH256(xrpl::sfNFTokenID)) {
            return Error{Status{XrpldError::RpcInvalidParams}};
        }

        startHint = sle->getFieldU64(xrpl::sfNFTokenOfferNode);
        output.offers.push_back(*sle);
        offers.reserve(reserve);
    } else {
        // We have no start point, limit should be one higher than requested.
        offers.reserve(++reserve);
    }

    auto result = traverseOwnedNodes(
        *sharedPtrBackend_,
        directory,
        cursor,
        startHint,
        lgrInfo.seq,
        reserve,
        yield,
        [&offers](xrpl::SLE&& offer) {
            if (offer.getType() == xrpl::ltNFTOKEN_OFFER) {
                offers.push_back(std::move(offer));
                return true;
            }

            return false;
        }
    );

    if (!result.has_value())
        return Error{result.error()};

    if (offers.size() == reserve) {
        output.limit = input.limit;
        output.marker = to_string(offers.back().key());
        offers.pop_back();
    }

    std::ranges::move(offers, std::back_inserter(output.offers));

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTOffersHandlerBase::Output const& output
)
{
    using boost::json::value_from;

    auto object = boost::json::object{
        {JS(nft_id), output.nftID},
        {JS(validated), output.validated},
        {JS(offers), value_from(output.offers)},
    };

    if (output.marker)
        object[JS(marker)] = *(output.marker);

    if (output.limit)
        object[JS(limit)] = *(output.limit);

    jv = std::move(object);
}

}  // namespace rpc
