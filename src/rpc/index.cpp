// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/context.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txdb.h>
#include <txmempool.h>
#include <univalue.h>
#include <validation.h>
#include <uint256.h>
#include <key_io.h>

#include <stdint.h>

#include <condition_variable>
#include <memory>
#include <mutex>

using node::NodeContext;

// Addressindex
bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b)
{
    return a.second.blockHeight < b.second.blockHeight;
}

bool timestampSort(std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> a,
                   std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> b)
{
    return a.second.time < b.second.time;
}


bool getAddressFromIndex(const int &type, const uint256 &hash, std::string &address)
{
    if (type == ADDR_INDT_SCRIPT_ADDRESS) {
        address = EncodeDestination(ScriptHash(uint160(hash.begin(), 20)));
    } else if (type == ADDR_INDT_PUBKEY_ADDRESS) {
        address = EncodeDestination(PKHash(uint160(hash.begin(), 20)));
    } else if (type == ADDR_INDT_WITNESS_V0_KEYHASH) {
        address = EncodeDestination(WitnessV0KeyHash(uint160(hash.begin(), 20)));
    } else if (type == ADDR_INDT_WITNESS_V0_SCRIPTHASH) {
        address = EncodeDestination(WitnessV0ScriptHash(hash));
    } else if (type == ADDR_INDT_WITNESS_V1_TAPROOT) {
        address = EncodeDestination(WitnessV1Taproot{XOnlyPubKey{hash}});
    } else {
        return false;
    }
    return true;
}

static bool getIndexKey(const std::string& str, uint256& hashBytes, int& type)
{
    CTxDestination dest = DecodeDestination(str);
    if (!IsValidDestination(dest)) {
        type = 0;
        return false;
    }

    if (dest.index() == DI::_PKHash) {
        const PKHash &id = std::get<PKHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_PUBKEY_ADDRESS;
        return true;
    }
    if (dest.index() == DI::_ScriptHash) {
        const ScriptHash& id = std::get<ScriptHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_SCRIPT_ADDRESS;
        return true;
    }
    if (dest.index() == DI::_WitnessV0KeyHash) {
        const WitnessV0KeyHash& id = std::get<WitnessV0KeyHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 20);
        type = ADDR_INDT_WITNESS_V0_KEYHASH;
        return true;
    }
    if (dest.index() == DI::_WitnessV0ScriptHash) {
        const WitnessV0ScriptHash& id = std::get<WitnessV0ScriptHash>(dest);
        memcpy(hashBytes.begin(), id.begin(), 32);
        type = ADDR_INDT_WITNESS_V0_SCRIPTHASH;
        return true;
    }
    if (dest.index() == DI::_WitnessV1Taproot) {
        const WitnessV1Taproot& id = std::get<WitnessV1Taproot>(dest);
        memcpy(hashBytes.begin(), id.begin(), 32);
        type = ADDR_INDT_WITNESS_V1_TAPROOT;
        return true;
    }
    type = ADDR_INDT_UNKNOWN;
    return false;
}

static bool getAddressesFromParams(const UniValue& params, std::vector<std::pair<uint256, int> > &addresses)
{
    if (params[0].isStr()) {
        uint256 hashBytes;
        int type = 0;
        if (!getIndexKey(params[0].get_str(), hashBytes, type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    } else if (params[0].isObject()) {

        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }

        std::vector<UniValue> values = addressValues.getValues();

        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {

            uint256 hashBytes;
            int type = 0;
            if (!getIndexKey(it->get_str(), hashBytes, type)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }
            addresses.push_back(std::make_pair(hashBytes, type));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    return true;
}


bool GetSpentIndex(ChainstateManager &chainman, const CSpentIndexKey &key, CSpentIndexValue &value, const CTxMemPool *pmempool)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    if (!fSpentIndex) {
        return false;
    }
    if (pmempool && pmempool->getSpentIndex(key, value)) {
        return true;
    }
    if (!pblocktree->ReadSpentIndex(key, value)) {
        return false;
    }

    return true;
};

bool GetAddressIndex(ChainstateManager &chainman, const uint256 &addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex, int start = 0, int end = 0)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};

    if (!fAddressIndex) {
        return error("Address index not enabled");
    }

    if (!pblocktree->ReadAddressIndex(addressHash, type, addressIndex, start, end)) {
        return error("Unable to get txids for address");
    }

    return true;
};


bool GetAddressUnspent(ChainstateManager &chainman, const uint256 &addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    if (!fAddressIndex) {
        return error("Address index not enabled");
    }
    if (!pblocktree->ReadAddressUnspentIndex(addressHash, type, unspentOutputs)) {
        return error("Unable to get txids for address");
    }

    return true;
};

static bool HashOnchainActive(ChainstateManager &chainman, const uint256 &hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlockIndex* pblockindex = chainman.m_blockman.LookupBlockIndex(hash);

    if (!chainman.ActiveChain().Contains(pblockindex)) {
        return false;
    }

    return true;
};

bool GetTimestampIndex(ChainstateManager &chainman, const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes)
{
    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    if (!fTimestampIndex) {
        return error("Timestamp index not enabled");
    }
    if (!pblocktree->ReadTimestampIndex(high, low, hashes)) {
        return error("Unable to get hashes for timestamps");
    }

    if (fActiveOnly) {
        for (auto it = hashes.begin(); it != hashes.end(); ) {
            if (!HashOnchainActive(chainman, it->first)) {
                it = hashes.erase(it);
            } else {
                ++it;
            }
        }
    }

    return true;
};

static RPCHelpMan getaddressbalance()
{
    return RPCHelpMan{"getaddressbalance",
                "\nReturns the balance for an address(es) (requires addressindex to be enabled).\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The Bitcoin address "},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_AMOUNT, "balance", "The current balance in satoshis"},
                        {RPCResult::Type::STR_AMOUNT, "received", "The total number of satoshis received (including change)"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddressbalance", "Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressbalance", "Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::vector<std::pair<uint256, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 7");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex(chainman, (*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    int nHeight = (unsigned int) chainman.ActiveChain().Height();

    CAmount balance = 0;
    CAmount balance_spendable = 0;
    CAmount balance_immature = 0;
    CAmount received = 0;

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        if (it->second > 0) {
            received += it->second;
        }
        if (it->first.txindex == 0 && nHeight - it->first.blockHeight < COINBASE_MATURITY) {
            balance_immature += it->second;
        } else {
            balance_spendable += it->second;
        }
        balance += it->second;
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("balance", balance);
    result.pushKV("balance_immature", balance_immature);
    result.pushKV("balance_spendable", balance_spendable);
    result.pushKV("received", received);

    return result;
},
    };
}


static RPCHelpMan getaddressesbalance()
{
    return RPCHelpMan{"getaddressesbalance",
                "\nReturns the balance for an address(es) (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_AMOUNT, "balance", "The current balance in satoshis"},
                        {RPCResult::Type::STR_AMOUNT, "received", "The total number of satoshis received (including change)"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddressesbalance", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressesbalance", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::vector<std::pair<uint256, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 7");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex(chainman, (*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    int nHeight = (unsigned int) chainman.ActiveChain().Height();

    CAmount balance = 0;
    CAmount balance_spendable = 0;
    CAmount balance_immature = 0;
    CAmount received = 0;

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        if (it->second > 0) {
            received += it->second;
        }
        if (it->first.txindex == 0 && nHeight - it->first.blockHeight < COINBASE_MATURITY) {
            balance_immature += it->second;
        } else {
            balance_spendable += it->second;
        }
        balance += it->second;
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("balance", balance);
    result.pushKV("balance_immature", balance_immature);
    result.pushKV("balance_spendable", balance_spendable);
    result.pushKV("received", received);

    return result;
},
    };
}


static RPCHelpMan getaddressutxos()
{
return RPCHelpMan{"getaddressutxos",
                "\nReturns all unspent outputs for an address (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                    {"chainInfo", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include chain info in results, only applies if start and end specified."},
                },
                {
                    RPCResult{"Default",
                        RPCResult::Type::ARR, "", "", {
                            {RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::STR, "address", "The base58check encoded address"},
                                {RPCResult::Type::STR_HEX, "txid", "The output txid"},
                                {RPCResult::Type::NUM, "height", "The block height"},
                                {RPCResult::Type::NUM, "outputIndex", "The output index"},
                                {RPCResult::Type::STR_HEX, "script", "The script hex encoded"},
                                {RPCResult::Type::NUM, "satoshis", "The number of satoshis of the output"},
                            }}
                        }
                    },
                    RPCResult{"With chainInfo", RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_HEX, "hash", "Start hash"},
                        {RPCResult::Type::NUM, "height", "Chain height"},
                        {RPCResult::Type::ARR, "utxos", "", {
                            {RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::ELISION, "", "Same as Default"},
                            }}
                        }}
                    }}
                },
                RPCExamples{
            HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager &chainman = EnsureAnyChainman(request.context);

    if (!fAddressIndex) {
        throw JSONRPCError(RPC_MISC_ERROR, "Address index is not enabled.");
    }

    bool includeChainInfo = false;
    if (request.params[0].isObject()) {
        UniValue chainInfo = find_value(request.params[0].get_obj(), "chainInfo");
        if (chainInfo.isBool()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }

    std::vector<std::pair<uint256, int> > addresses;
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressUnspent(chainman, it->first, it->second, unspentOutputs)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.pushKV("address", address);
        output.pushKV("txid", it->first.txhash.GetHex());
        output.pushKV("outputIndex", int(it->first.index));
        output.pushKV("script", HexStr(it->second.script));
        output.pushKV("satoshis", it->second.satoshis);
        output.pushKV("height", it->second.blockHeight);
        utxos.push_back(output);
    }

    if (includeChainInfo) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("utxos", utxos);

        LOCK(cs_main);
        result.pushKV("hash", chainman.ActiveChain().Tip()->GetBlockHash().GetHex());
        result.pushKV("height", int(chainman.ActiveChain().Height()));
        return result;
    } else {
        return utxos;
    }
},
    };
}


static RPCHelpMan getaddressdeltas()
{
    return RPCHelpMan{"getaddressdeltas",
                "\nReturns all changes for an address (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                    {"start", RPCArg::Type::NUM, RPCArg::Default{0}, "The start block height."},
                    {"end", RPCArg::Type::NUM, RPCArg::Default{0}, "The end block height."},
                    {"chainInfo", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include chain info in results, only applies if start and end specified."},
                },
                {
                    RPCResult{"Default",
                        RPCResult::Type::ARR, "", "", {
                            {RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::NUM, "satoshis", "The difference of satoshis"},
                                {RPCResult::Type::STR_HEX, "txid", "The related txid"},
                                {RPCResult::Type::NUM, "index", "The block height"},
                                {RPCResult::Type::NUM, "blockindex", "The index of the transaction in the block"},
                                {RPCResult::Type::NUM, "height", "The block height"},
                                {RPCResult::Type::STR, "address", "The base58check encoded address"},
                            }}
                        }
                    },
                    RPCResult{"With chainInfo", RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::ARR, "deltas", "", {
                            {RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::ELISION, "", "Same output as Default output"},
                            }}
                        }},
                        {RPCResult::Type::OBJ, "start", "", {
                            {RPCResult::Type::STR_HEX, "hash", "Start hash"},
                            {RPCResult::Type::NUM, "height", "Start height"},
                        }},
                        {RPCResult::Type::OBJ, "end", "", {
                            {RPCResult::Type::STR_HEX, "hash", "End hash"},
                            {RPCResult::Type::NUM, "height", "End height"},
                        }},
                    }}
                },
                RPCExamples{
            HelpExampleCli("getaddressdeltas", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressdeltas", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    if (!fAddressIndex) {
        throw JSONRPCError(RPC_MISC_ERROR, "Address index is not enabled.");
    }

    ChainstateManager &chainman = EnsureAnyChainman(request.context);

    UniValue startValue = find_value(request.params[0].get_obj(), "start");
    UniValue endValue = find_value(request.params[0].get_obj(), "end");

    UniValue chainInfo = find_value(request.params[0].get_obj(), "chainInfo");
    bool includeChainInfo = false;
    if (chainInfo.isBool()) {
        includeChainInfo = chainInfo.get_bool();
    }

    int start = 0;
    int end = 0;
    
    if (startValue.isNum() && endValue.isNum()) {
        start = startValue.getInt<int>();
        end = endValue.getInt<int>();
        if (start <= 0 || end <= 0) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start and end is expected to be greater than zero");
        }
        if (end < start) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "End value is expected to be greater than start");
        }
    }

    std::vector<std::pair<uint256, int> > addresses;
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    UniValue deltas(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("satoshis", it->second);
        delta.pushKV("txid", it->first.txhash.GetHex());
        delta.pushKV("index", int(it->first.index));
        delta.pushKV("blockindex", int(it->first.txindex));
        delta.pushKV("height", it->first.blockHeight);
        delta.pushKV("address", address);
        deltas.push_back(delta);
    }

    UniValue result(UniValue::VOBJ);

    if (includeChainInfo && start > 0 && end > 0) {
        LOCK(cs_main);
        const int tip_height = chainman.ActiveChain().Height();
        if (start > tip_height || end > tip_height) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start or end is outside chain range");
        }

        CBlockIndex* startIndex = chainman.ActiveChain()[start];
        CBlockIndex* endIndex = chainman.ActiveChain()[end];

        UniValue startInfo(UniValue::VOBJ);
        UniValue endInfo(UniValue::VOBJ);

        startInfo.pushKV("hash", startIndex->GetBlockHash().GetHex());
        startInfo.pushKV("height", start);

        endInfo.pushKV("hash", endIndex->GetBlockHash().GetHex());
        endInfo.pushKV("height", end);

        result.pushKV("deltas", deltas);
        result.pushKV("start", startInfo);
        result.pushKV("end", endInfo);

        return result;
    } else {
        return deltas;
    }
},
    };
}


static RPCHelpMan getaddresstxids()
{
    return RPCHelpMan{"getaddresstxids",
                "\nReturns the txids for an address(es) (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                    {"start", RPCArg::Type::NUM, RPCArg::Default{0}, "The start block height."},
                    {"end", RPCArg::Type::NUM, RPCArg::Default{0}, "The end block height."},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::STR_HEX, "transactionid", "The transaction txid"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    if (!fAddressIndex) {
        throw JSONRPCError(RPC_MISC_ERROR, "Address index is not enabled.");
    }
    ChainstateManager &chainman = EnsureAnyChainman(request.context);

    std::vector<std::pair<uint256, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    int start = 0;
    int end = 0;
    if (request.params[0].isObject()) {
        UniValue startValue = find_value(request.params[0].get_obj(), "start");
        UniValue endValue = find_value(request.params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.getInt<int>();
            end = endValue.getInt<int>();
        }
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint256, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex(chainman, it->first, it->second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    std::set<std::pair<int, std::string> > txids;
    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        int height = it->first.blockHeight;
        std::string txid = it->first.txhash.GetHex();

        if (addresses.size() > 1) {
            txids.insert(std::make_pair(height, txid));
        } else {
            if (txids.insert(std::make_pair(height, txid)).second) {
                result.push_back(txid);
            }
        }
    }

    if (addresses.size() > 1) {
        for (std::set<std::pair<int, std::string> >::const_iterator it=txids.begin(); it!=txids.end(); it++) {
            result.push_back(it->second);
        }
    }

    return result;
},
    };
}


static RPCHelpMan getblockhashes()
{
    return RPCHelpMan{"getblockhashes",
                "\nReturns array of hashes of blocks within the timestamp range provided.\n",
                {
                    {"high", RPCArg::Type::NUM, RPCArg::Optional::NO, "The newer block timestamp."},
                    {"low", RPCArg::Type::NUM, RPCArg::Optional::NO, "The older block timestamp."},
                    {"options", RPCArg::Type::OBJ, RPCArg::Default{UniValue::VOBJ}, "",
                        {
                            {"noOrphans", RPCArg::Type::BOOL, RPCArg::Default{false}, "Only include blocks on the main chain."},
                            {"logicalTimes", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include logical timestamps with hashes."},
                        },
                    },
                },
                RPCResults{
                    {RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::STR_HEX, "hash", "The block hash"},
                    }},
                    {RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::OBJ, "", "", {
                            {RPCResult::Type::STR_HEX, "blockhash", "The block hash"},
                            {RPCResult::Type::NUM, "logicalts", "The logical timestamp"},
                            {RPCResult::Type::NUM, "height", "The height of the block containing the spending tx"},
                        }}
                    }}
                },
                RPCExamples{
            HelpExampleCli("getblockhashes", "1231614698 1231024505 '{\"noOrphans\":false, \"logicalTimes\":true}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getblockhashes", "1231614698, 1231024505")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);

    unsigned int high = request.params[0].getInt<int>();
    unsigned int low = request.params[1].getInt<int>();
    bool fActiveOnly = false;
    bool fLogicalTS = false;

    if (request.params.size() > 2) {
        if (request.params[2].isObject()) {
            UniValue noOrphans = find_value(request.params[2].get_obj(), "noOrphans");
            UniValue returnLogical = find_value(request.params[2].get_obj(), "logicalTimes");

            if (noOrphans.isBool()) {
                fActiveOnly = noOrphans.get_bool();
            }
            if (returnLogical.isBool()) {
                fLogicalTS = returnLogical.get_bool();
            }
        }
    }

    std::vector<std::pair<uint256, unsigned int> > blockHashes;

    {
        LOCK(cs_main);
        if (!GetTimestampIndex(chainman, high, low, fActiveOnly, blockHashes)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for block hashes");
        }
    }

    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<uint256, unsigned int> >::const_iterator it=blockHashes.begin(); it!=blockHashes.end(); it++) {
        if (fLogicalTS) {
            UniValue item(UniValue::VOBJ);
            item.pushKV("blockhash", it->first.GetHex());
            item.pushKV("logicalts", int(it->second));
            result.push_back(item);
        } else {
            result.push_back(it->first.GetHex());
        }
    }

    return result;
},
    };
}


static RPCHelpMan getaddressmempool()
{
    return RPCHelpMan{"getaddressmempool",
                "\nReturns all mempool deltas for an address (requires addressindex to be enabled).\n",
                {
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::NO, "A json array with addresses.\n",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The base58check encoded address."},
                        },
                    RPCArgOptions{.skip_type_check = true}},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "", {
                        {RPCResult::Type::OBJ, "", "", {
                            {RPCResult::Type::STR, "address", "The base58check encoded address"},
                            {RPCResult::Type::STR_HEX, "txid", "The related txids"},
                            {RPCResult::Type::NUM, "index", "The related input or output index"},
                            {RPCResult::Type::NUM, "satoshis", "The difference of satoshis"},
                            {RPCResult::Type::NUM_TIME, "timestamp", "The time the transaction entered the mempool (seconds)"},
                            {RPCResult::Type::STR_HEX, "prevtxid", /*optional=*/true, "The previous txid (if spending)"},
                            {RPCResult::Type::NUM, "prevout", /*optional=*/true, "The previous transaction output index (if spending)"},
                        }}
                    }
                },
                RPCExamples{
            HelpExampleCli("getaddressmempool", "'{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getaddressmempool", "{\"addresses\": [\"Pb7FLL3DyaAVP2eGfRiEkj4U8ZJ3RHLY9g\"]}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    node::NodeContext &node = EnsureAnyNodeContext(request.context);
    const CTxMemPool& mempool = EnsureMemPool(node);

    if (!fAddressIndex) {
        throw JSONRPCError(RPC_MISC_ERROR, "Address index is not enabled.");
    }

    std::vector<std::pair<uint256, int> > addresses;
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> > indexes;
    if (!mempool.getAddressIndex(addresses, indexes)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
    }

    std::sort(indexes.begin(), indexes.end(), timestampSort);

    UniValue result(UniValue::VARR);

    for (std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> >::iterator it = indexes.begin();
         it != indexes.end(); it++) {

        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.addressBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        UniValue delta(UniValue::VOBJ);
        delta.pushKV("address", address);
        delta.pushKV("txid", it->first.txhash.GetHex());
        delta.pushKV("index", int(it->first.index));
        delta.pushKV("satoshis", it->second.amount);
        delta.pushKV("timestamp", it->second.time);
        if (it->second.amount < 0) {
            delta.pushKV("prevtxid", it->second.prevhash.GetHex());
            delta.pushKV("prevout", int(it->second.prevout));
        }
        result.push_back(delta);
    }

    return result;
},
    };
}


static RPCHelpMan getspentinfo()
{
    return RPCHelpMan{"getspentinfo",
                "\nReturns the txid and index where an output is spent.\n",
                {
                    {"inputs", RPCArg::Type::OBJ, RPCArg::Optional::NO, "",
                        {
                            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The hex string of the txid."},
                            {"index", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number."},
                        },
                    },
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
                        {RPCResult::Type::NUM, "index", "The spending input index"},
                        {RPCResult::Type::NUM, "height", "The height of the block containing the spending tx"},
                    }
                },
                RPCExamples{
            HelpExampleCli("getspentinfo", "'{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}'") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getspentinfo", "{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    node::NodeContext &node = EnsureAnyNodeContext(request.context);
    const CTxMemPool& mempool = EnsureMemPool(node);
    ChainstateManager &chainman = EnsureChainman(node);

    UniValue txidValue = find_value(request.params[0].get_obj(), "txid");
    UniValue indexValue = find_value(request.params[0].get_obj(), "index");

    if (!txidValue.isStr() || !indexValue.isNum()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid txid or index");
    }

    uint256 txid = ParseHashV(txidValue, "txid");
    int outputIndex = indexValue.getInt<int>();

    CSpentIndexKey key(txid, outputIndex);
    CSpentIndexValue value;

    if (!GetSpentIndex(chainman, key, value, &mempool)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("txid", value.txid.GetHex());
    obj.pushKV("index", int(value.inputIndex));
    obj.pushKV("height", value.blockHeight);

    return obj;
},
    };
}


void RegisterIndexRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        // Address index
        {"getaddressesbalance", &getaddressesbalance},
        {"getaddressmempool", &getaddressmempool},
        {"getaddressbalance", &getaddressbalance},
        {"getaddressdeltas",  &getaddressdeltas},
        {"getaddressutxos",   &getaddressutxos},
        {"getaddresstxids",   &getaddresstxids},
        {"getblockhashes",    &getblockhashes},
        {"getspentinfo",      &getspentinfo},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
