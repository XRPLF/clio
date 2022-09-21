#ifndef REPORTING_HANDLERS_H_INCLUDED
#define REPORTING_HANDLERS_H_INCLUDED

#include <rpc/RPC.h>

namespace RPC {
/*
 * This file just contains declarations for all of the handlers
 */

// account state methods
Result
doAccountInfo(Context const& context);

Result
doAccountChannels(Context const& context);

Result
doAccountCurrencies(Context const& context);

Result
doAccountLines(Context const& context);

Result
doAccountNFTs(Context const& context);

Result
doAccountObjects(Context const& context);

Result
doAccountOffers(Context const& context);

Result
doGatewayBalances(Context const& context);

Result
doNoRippleCheck(Context const& context);

// channels methods

Result
doChannelAuthorize(Context const& context);

Result
doChannelVerify(Context const& context);

// book methods
Result
doBookChanges(Context const& context);

Result
doBookOffers(Context const& context);

// NFT methods
Result
doNFTBuyOffers(Context const& context);

Result
doNFTSellOffers(Context const& context);

Result
doNFTInfo(Context const& context);

Result
doNFTHistory(Context const& context);

// ledger methods
Result
doLedger(Context const& context);

Result
doLedgerEntry(Context const& context);

Result
doLedgerData(Context const& context);

Result
doLedgerRange(Context const& context);

// transaction methods
Result
doTx(Context const& context);

Result
doTransactionEntry(Context const& context);

Result
doAccountTx(Context const& context);

// subscriptions
Result
doSubscribe(Context const& context);

Result
doUnsubscribe(Context const& context);

// server methods
Result
doServerInfo(Context const& context);

// Utility methods
Result
doRandom(Context const& context);
}  // namespace RPC
#endif
