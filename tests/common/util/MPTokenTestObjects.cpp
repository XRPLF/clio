#include "util/MPTokenTestObjects.hpp"

#include "util/TestObject.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <string_view>
#include <utility>

namespace util {

xrpl::STObject
createMPTokenNode(
    xrpl::SField const& nodeType,
    xrpl::uint192 const& issuanceID,
    std::string_view holder
)
{
    auto const& fieldsName =
        nodeType == xrpl::sfCreatedNode ? xrpl::sfNewFields : xrpl::sfFinalFields;

    xrpl::STObject fields(fieldsName);
    fields.setAccountID(xrpl::sfAccount, ::getAccountIdWithString(holder));
    fields[xrpl::sfMPTokenIssuanceID] = issuanceID;

    xrpl::STObject node(nodeType);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

xrpl::STObject
createMPTokenIssuanceNode(xrpl::SField const& nodeType, std::uint32_t seq, std::string_view issuer)
{
    auto const& fieldsName =
        nodeType == xrpl::sfCreatedNode ? xrpl::sfNewFields : xrpl::sfFinalFields;

    xrpl::STObject fields(fieldsName);
    fields.setFieldU32(xrpl::sfSequence, seq);
    fields.setAccountID(xrpl::sfIssuer, ::getAccountIdWithString(issuer));

    xrpl::STObject node(nodeType);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

}  // namespace util
