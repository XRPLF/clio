#include "rpc/handlers/NFTInfo.hpp"

#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/Errors.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nft.h>

#include <string>

using namespace xrpl;

namespace rpc {

NFTInfoHandler::Result
NFTInfoHandler::process(NFTInfoHandler::Input const& input, Context const& ctx) const
{
    auto const& tokenID = input.nftID;
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "NFTInfo's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;
    auto const maybeNft = sharedPtrBackend_->fetchNFT(tokenID, lgrInfo.seq, ctx.yield);

    if (not maybeNft.has_value())
        return Error{Status{XrpldError::RpcObjectNotFound, "NFT not found"}};

    // TODO - this formatting is exactly the same and SHOULD REMAIN THE SAME
    // for each element of the `nfts_by_issuer` API. We should factor this out
    // so that the formats don't diverge. In the mean time, do not make any
    // changes to this formatting without making the same changes to that
    // formatting.
    auto const& nft = *maybeNft;
    auto output = NFTInfoHandler::Output{};

    output.nftID = strHex(nft.tokenID);
    output.ledgerIndex = nft.ledgerSequence;
    output.owner = toBase58(nft.owner);
    output.isBurned = nft.isBurned;
    output.flags = nft::getFlags(nft.tokenID);
    output.transferFee = nft::getTransferFee(nft.tokenID);
    output.issuer = toBase58(nft::getIssuer(nft.tokenID));
    output.taxon = nft::toUInt32(nft::getTaxon(nft.tokenID));
    output.serial = nft::getSequence(nft.tokenID);
    output.uri = strHex(nft.uri);

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTInfoHandler::Output const& output
)
{
    // TODO: use JStrings when they become available
    jv = boost::json::object{
        {JS(nft_id), output.nftID},
        {JS(ledger_index), output.ledgerIndex},
        {JS(owner), output.owner},
        {JS(is_burned), output.isBurned},
        {JS(flags), output.flags},
        {"transfer_fee", output.transferFee},
        {JS(issuer), output.issuer},
        {JS(nft_taxon), output.taxon},
        {JS(nft_serial), output.serial},
        {JS(validated), output.validated},
        {JS(uri), output.uri},
    };
}

}  // namespace rpc
