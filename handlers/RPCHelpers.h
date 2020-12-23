
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED

#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STTx.h>
#include <boost/json.hpp>
std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account);

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(
    std::pair<std::vector<unsigned char>, std::vector<unsigned char>> const&
        blobs);

boost::json::object
getJson(ripple::STBase const& obj);

boost::json::object
getJson(ripple::SLE const& sle);

#endif
