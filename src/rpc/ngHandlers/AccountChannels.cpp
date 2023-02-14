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

#include <rpc/ngHandlers/AccountChannels.h>

namespace RPCng {

std::variant<RPC::Status, ripple::LedgerInfo>
AccountChannelsHandler::getLedgerInfoFromHashOrSeq(
    std::optional<std::string> ledgerHash,
    std::optional<uint32_t> ledgerIndex,
    uint32_t maxSeq)
{
    if (!ledgerHash)
    {
        auto lgrInfo =
            sharedPtrBackend_->fetchLedgerByHash(ledgerHash, ctx.yield);

        if (!lgrInfo || lgrInfo->seq > maxSeq)
            return Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

        return *lgrInfo;
    }

    uint64_t ledgerSequence = ledgerIndex ? ledgerIndex.value() : maxSeq;

    lgrInfo =
        sharedPtrBackend_->fetchLedgerBySequence(*ledgerSequence, yieldCtx_);

    if (!lgrInfo || lgrInfo->seq > maxSeq)
        return RPC::Status{
            RPC::RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

    return *lgrInfo;
}

AccountChannelsHandler::Result
AccountChannelsHandler::process(AccountChannelsHandler::Input input) const
{
    std::optional<ripple::LedgerInfo> lgrInfo;
    auto range = sharedPtrBackend_->fetchLedgerRange();
    assert(range);
    if (input.ledgerHash)
    {
        ripple::uint256 ledgerHash{input.ledgerHash.value().c_str()};
        lgrInfo = sharedPtrBackend_->fetchLedgerByHash(ledgerHash, yieldCtx_);
        if (!lgrInfo || lgrInfo->seq > range->maxSequence)
            return RPCng::Error{RPC::Status{
                RPC::RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"}};
    }

    return Output{};
}
}  // namespace RPCng
