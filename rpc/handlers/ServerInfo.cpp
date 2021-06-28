#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>
boost::json::object
doServerInfo(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;

    auto rng = backend.fetchLedgerRange();
    if (!rng)
    {
        response["complete_ledgers"] = "empty";
    }
    else
    {
        std::string completeLedgers = std::to_string(rng->minSequence);
        if (rng->maxSequence != rng->minSequence)
            completeLedgers += "-" + std::to_string(rng->maxSequence);
        response["complete_ledgers"] = completeLedgers;
    }
    if (rng)
    {
        auto lgrInfo = backend.fetchLedgerBySequence(rng->maxSequence);
        response["validated_ledger"] = toJson(*lgrInfo);
    }

    boost::json::array indexes;

    if (rng)
    {
        uint32_t cur = rng->minSequence;
        while (cur <= rng->maxSequence + 1)
        {
            auto keyIndex = backend.getKeyIndexOfSeq(cur);
            assert(keyIndex.has_value());
            cur = keyIndex->keyIndex;
            boost::json::object entry;
            entry["complete"] = backend.isLedgerIndexed(cur);
            entry["sequence"] = cur;
            indexes.emplace_back(entry);
            cur = cur + 1;
        }
    }
    response["indexes"] = indexes;
    auto indexing = backend.getIndexer().getCurrentlyIndexing();
    if (indexing)
        response["indexing"] = *indexing;
    else
        response["indexing"] = "none";

    return response;
}
