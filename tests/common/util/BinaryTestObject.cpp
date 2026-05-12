#include "util/BinaryTestObject.hpp"

#include "data/DBHelpers.hpp"
#include "etl/Models.hpp"
#include "etl/impl/Extraction.hpp"
#include "util/StringUtils.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/digest.h>

#include <optional>
#include <string>
#include <utility>

namespace {

constinit auto const kSEQ = 30;
constinit auto const kRAW_HEADER =
    "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351E"
    "DD733898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A"
    "6DB6FE30CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
    "3E2232B33EF57CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58"
    "CE5AA29652EFFD80AC59CD91416E4E13DBBE";

}  // namespace

namespace util {

std::pair<std::string, std::string>
createTxAndMetaBlobs(std::string metaStr, std::string txnStr)
{
    return {hexStringToBinaryString(metaStr), hexStringToBinaryString(txnStr)};
}

std::pair<ripple::STTx, ripple::TxMeta>
createTxAndMeta(std::string hashStr, std::string metaStr, std::string txnStr)
{
    ripple::uint256 hash;
    EXPECT_TRUE(hash.parseHex(hashStr));

    auto const [metaBlob, txnBlob] = createTxAndMetaBlobs(metaStr, txnStr);

    ripple::SerialIter it{txnBlob.data(), txnBlob.size()};
    return {ripple::STTx{it}, ripple::TxMeta{hash, kSEQ, metaBlob}};
}

etl::model::Transaction
createTransaction(ripple::TxType type, std::string hashStr, std::string metaStr, std::string txnStr)
{
    auto const [sttx, meta] = createTxAndMeta(hashStr, metaStr, txnStr);
    return {
        .raw = "",
        .metaRaw = "",
        .sttx = sttx,
        .meta = meta,
        .id = ripple::uint256{"0000000000000000000000000000000000000000000000000000000000000001"},
        .key = "0000000000000000000000000000000000000000000000000000000000000001",
        .type = type
    };
}

etl::model::Object
createObject(etl::model::Object::ModType modType, std::string key)
{
    // random object taken from initial ledger load
    static constinit auto const kOBJ_PRED =
        "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960A";
    static constinit auto const kOBJ_SUCC =
        "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960F";
    static constinit auto const kOBJ_BLOB =
        "11007222002200002504270918370000000000000C4538000000000000000A554D94799200CC37EFAF45DA7670"
        "4ED3CBEDBB4B4FCD"
        "56E9CBA5399EB40A7B3BEC629546DD24CDB4C0004C4A5059000000000000000000000000000000000000000000"
        "0000000000000000"
        "000000000000016680000000000000004C4A505900000000000000000000000000000000368480B7780E3DCF5D"
        "062A7BB54129F42F"
        "8BB63367D6C38D7EA4C680004C4A505900000000000000000000000000000000C8056BA4E36038A8A0D2C0A869"
        "63153E95A84D56";

    return {
        .key = binaryStringToUint256(hexStringToBinaryString(key)),
        .keyRaw = hexStringToBinaryString(key),
        .data = modType == etl::model::Object::ModType::Deleted
            ? ripple::Blob{}
            : *ripple::strUnHex(kOBJ_BLOB),  // NOLINT(bugprone-unchecked-optional-access)
        .dataRaw = modType == etl::model::Object::ModType::Deleted
            ? ""
            : hexStringToBinaryString(kOBJ_BLOB),
        .successor = hexStringToBinaryString(kOBJ_SUCC),
        .predecessor = hexStringToBinaryString(kOBJ_PRED),
        .type = modType,
    };
}

etl::model::Object
createObjectWithBookBase(etl::model::Object::ModType modType, std::string key)
{
    // random object taken from initial ledger load
    static constinit auto const kOBJ_PRED =
        "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960A";
    static constinit auto const kOBJ_SUCC =
        "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960F";
    static constinit auto const kOBJ_BLOB =
        "11006422000000022505A681E855B4E076DD06D6D583804F9DC94F641337ECB97F71860300EEC17E530A2001D6"
        "C9583FFBFAD704E299BE"
        "3E544090ECCB12AF45FD03CAEEA852E5048E57F48FD45B505A0008138882D0F98C64A1A0E6D15053589771AD08"
        "B8C13D5384FBDAE20000"
        "0948011320AC38AE866862CF5A8AF3578C600CEE8BFB894596584B60C0FFA7D22248E33CC3";

    return {
        .key = binaryStringToUint256(hexStringToBinaryString(key)),
        .keyRaw = hexStringToBinaryString(key),
        .data = modType == etl::model::Object::ModType::Deleted
            ? ripple::Blob{}
            : *ripple::strUnHex(kOBJ_BLOB),  // NOLINT(bugprone-unchecked-optional-access)
        .dataRaw = modType == etl::model::Object::ModType::Deleted
            ? ""
            : hexStringToBinaryString(kOBJ_BLOB),
        .successor = hexStringToBinaryString(kOBJ_SUCC),
        .predecessor = hexStringToBinaryString(kOBJ_PRED),
        .type = modType,
    };
}

etl::model::Object
createObjectWithTwoNFTs()
{
    std::string const url1 = "abcd1";
    std::string const url2 = "abcd2";
    ripple::Blob const uri1Blob(url1.begin(), url1.end());
    ripple::Blob const uri2Blob(url2.begin(), url2.end());

    constexpr auto kACCOUNT = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
    constexpr auto kNFT_ID = "0008013AE1CD8B79A8BCB52335CD40DE97401B2D60A828720000099B00000000";
    constexpr auto kNFT_ID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";

    auto const nftPage = createNftTokenPage({{kNFT_ID, url1}, {kNFT_ID2, url2}}, std::nullopt);
    auto const serializerNftPage = nftPage.getSerializer();
    auto const account = getAccountIdWithString(kACCOUNT);

    // key is a token made up from owner's account ID followed by unused (in Clio) value described
    // here:
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0020-non-fungible-tokens#tokenpage-id-format
    constexpr auto kEXTRA_BYTES = "000000000000";
    auto const key = std::string(std::begin(account), std::end(account)) + kEXTRA_BYTES;

    return {
        .key = {},
        .keyRaw = key,
        .data = {},
        .dataRaw = std::string(
            static_cast<char const*>(serializerNftPage.getDataPtr()),
            serializerNftPage.getDataLength()
        ),
        .successor = "",
        .predecessor = "",
        .type = etl::model::Object::ModType::Created,
    };
}

etl::model::Object
createObjectWithMPT()
{
    constexpr auto kACCOUNT = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";

    auto const account = getAccountIdWithString(kACCOUNT);
    auto const mptID = ripple::makeMptID(2, getAccountIdWithString(kACCOUNT));
    auto const mptokenObject = createMpTokenObject(kACCOUNT, mptID);

    // key is a token made up from several fields described here:
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0033-multi-purpose-tokens#2121-mptoken-ledger-identifier
    constexpr auto kSPACE_KEY = 0x007F;
    auto const keySha512Half = ripple::sha512Half(kSPACE_KEY, mptID, account);

    return {
        .key = {},
        .keyRaw = std::string(std::begin(keySha512Half), std::end(keySha512Half)),
        .data = {},
        .dataRaw = std::string(
            static_cast<char const*>(mptokenObject.getSerializer().getDataPtr()),
            mptokenObject.getSerializer().getDataLength()
        ),
        .successor = "",
        .predecessor = "",
        .type = etl::model::Object::ModType::Created,
    };
}

etl::model::BookSuccessor
createSuccessor()
{
    return {
        .firstBook = uint256ToString(
            ripple::uint256{"A000000000000000000000000000000000000000000000000000000000000000"}
        ),
        .bookBase = uint256ToString(
            ripple::uint256{"A000000000000000000000000000000000000000000000000000000000000001"}
        ),
    };
}

etl::impl::PBLedgerResponseType
createDataAndDiff()
{
    auto const rawHeaderBlob = hexStringToBinaryString(kRAW_HEADER);

    auto res = etl::impl::PBLedgerResponseType();
    res.set_ledger_header(rawHeaderBlob);
    res.set_objects_included(true);
    res.set_object_neighbors_included(true);

    {
        auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
        auto const [metaRaw, txRaw] = createTxAndMetaBlobs();
        original.set_transaction_blob(txRaw);
        original.set_metadata_blob(metaRaw);
        for (int i = 0; i < 10; ++i) {
            auto* p = res.mutable_transactions_list()->add_transactions();
            *p = original;
        }
    }
    {
        auto expected = createObject();
        auto original = org::xrpl::rpc::v1::RawLedgerObject();
        original.set_data(expected.dataRaw);
        original.set_key(expected.keyRaw);
        for (auto i = 0; i < 10; ++i) {
            auto* p = res.mutable_ledger_objects()->add_objects();
            *p = original;
        }
    }
    {
        auto expected = createSuccessor();
        auto original = org::xrpl::rpc::v1::BookSuccessor();
        original.set_first_book(expected.firstBook);
        original.set_book_base(expected.bookBase);

        res.set_object_neighbors_included(true);
        for (auto i = 0; i < 10; ++i) {
            auto* p = res.mutable_book_successors()->Add();
            *p = original;
        }
    }

    return res;
}

etl::impl::PBLedgerResponseType
createData()
{
    auto const rawHeaderBlob = hexStringToBinaryString(kRAW_HEADER);

    auto res = etl::impl::PBLedgerResponseType();
    res.set_ledger_header(rawHeaderBlob);
    res.set_objects_included(false);
    res.set_object_neighbors_included(false);

    {
        auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
        auto const [metaRaw, txRaw] = createTxAndMetaBlobs();
        original.set_transaction_blob(txRaw);
        original.set_metadata_blob(metaRaw);
        for (int i = 0; i < 10; ++i) {
            auto* p = res.mutable_transactions_list()->add_transactions();
            *p = original;
        }
    }

    return res;
}

}  // namespace util
