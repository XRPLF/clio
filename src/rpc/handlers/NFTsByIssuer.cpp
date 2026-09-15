#include "rpc/handlers/NFTsByIssuer.hpp"

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
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nft.h>

#include <optional>
#include <string>

using namespace xrpl;

namespace rpc {

NFTsByIssuerHandler::Result
NFTsByIssuerHandler::process(NFTsByIssuerHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "NFTsByIssuer's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );
    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    auto const limit = input.limit;

    auto const& issuer = input.issuer;
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        xrpl::keylet::account(issuer).key, lgrInfo.seq, ctx.yield
    );

    if (!accountLedgerObject)
        return Error{Status{RippledError::RpcActNotFound}};

    auto const cursor = input.marker;

    auto const dbResponse = sharedPtrBackend_->fetchNFTsByIssuer(
        issuer, input.nftTaxon, lgrInfo.seq, limit, cursor, ctx.yield
    );

    auto output = NFTsByIssuerHandler::Output{};

    output.issuer = toBase58(issuer);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;
    output.nftTaxon = input.nftTaxon;

    for (auto const& nft : dbResponse.nfts) {
        boost::json::object nftJson;

        nftJson[JS(nft_id)] = strHex(nft.tokenID);
        nftJson[JS(ledger_index)] = nft.ledgerSequence;
        nftJson[JS(owner)] = toBase58(nft.owner);
        nftJson[JS(is_burned)] = nft.isBurned;
        nftJson[JS(uri)] = strHex(nft.uri);

        nftJson[JS(flags)] = nft::getFlags(nft.tokenID);
        nftJson["transfer_fee"] = nft::getTransferFee(nft.tokenID);
        nftJson[JS(issuer)] = toBase58(nft::getIssuer(nft.tokenID));
        nftJson[JS(nft_taxon)] = nft::toUInt32(nft::getTaxon(nft.tokenID));
        nftJson[JS(nft_serial)] = nft::getSequence(nft.tokenID);

        output.nfts.push_back(nftJson);
    }

    if (dbResponse.cursor.has_value())
        output.marker = strHex(*dbResponse.cursor);

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTsByIssuerHandler::Output const& output
)
{
    jv = {
        {JS(issuer), output.issuer},
        {JS(limit), output.limit},
        {JS(ledger_index), output.ledgerIndex},
        {"nfts", output.nfts},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = *(output.marker);

    if (output.nftTaxon.has_value())
        jv.as_object()[JS(nft_taxon)] = *(output.nftTaxon);
}

}  // namespace rpc
