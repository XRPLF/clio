
#ifndef XRPL_REPORTING_RPCHELPERS_H_INCLUDED
#define XRPL_REPORTING_RPCHELPERS_H_INCLUDED

#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>
std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    boost::optional<ripple::AccountID> result;

    auto const publicKey = ripple::parseBase58<ripple::PublicKey>(
        ripple::TokenType::AccountPublic, account);

    if (publicKey)
        result = ripple::calcAccountID(*publicKey);
    else
        result = ripple::parseBase58<ripple::AccountID>(account);

    if (result)
        return result.value();
    else
        return {};
}
boost::json::object
getJson(ripple::SLE const& sle)
{
    auto start = std::chrono::system_clock::now();
    boost::json::value value = boost::json::parse(
        sle.getJson(ripple::JsonOptions::none).toStyledString());
    auto end = std::chrono::system_clock::now();
    value.as_object()["deserialization_time_microseconds"] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    return value.as_object();
}

#endif
