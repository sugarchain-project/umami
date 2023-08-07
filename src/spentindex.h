// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SPENTINDEX_H
#define BITCOIN_SPENTINDEX_H

#include <uint256.h>
#include <consensus/amount.h>
#include <script/script.h>
#include <serialize.h>

struct CSpentIndexKey {
    uint256 txid;
    unsigned int outputIndex;

    SERIALIZE_METHODS(CSpentIndexKey, obj)
    {
        READWRITE(obj.txid, obj.outputIndex);
    }

    CSpentIndexKey(uint256 t, unsigned int i) {
        txid = t;
        outputIndex = i;
    }

    CSpentIndexKey() {
        SetNull();
    }

    void SetNull() {
        txid.SetNull();
        outputIndex = 0;
    }

};

struct CSpentIndexValue {
    uint256 txid;
    unsigned int inputIndex;
    int blockHeight;
    CAmount satoshis;
    int addressType;
    uint256 addressHash;

    SERIALIZE_METHODS(CSpentIndexValue, obj)
    {
        READWRITE(obj.txid, obj.inputIndex, obj.blockHeight, obj.satoshis, obj.addressType, obj.addressHash);
    }

    CSpentIndexValue(uint256 t, unsigned int i, int h, CAmount s, int type, uint256 a) {
        txid = t;
        inputIndex = i;
        blockHeight = h;
        satoshis = s;
        addressType = type;
        addressHash = a;
    }

    CSpentIndexValue() {
        SetNull();
    }

    void SetNull() {
        txid.SetNull();
        inputIndex = 0;
        blockHeight = 0;
        satoshis = 0;
        addressType = 0;
        addressHash.SetNull();
    }

    bool IsNull() const {
        return txid.IsNull();
    }
};

struct CSpentIndexKeyCompare
{
    bool operator()(const CSpentIndexKey& a, const CSpentIndexKey& b) const {
        if (a.txid == b.txid) {
            return a.outputIndex < b.outputIndex;
        } else {
            return a.txid < b.txid;
        }
    }
};

struct CSpentIndexTxInfo
{
    std::map<CSpentIndexKey, CSpentIndexValue, CSpentIndexKeyCompare> mSpentInfo;
};

struct CTimestampIndexIteratorKey {
    unsigned int timestamp;

    SERIALIZE_METHODS(CTimestampIndexIteratorKey, obj) { READWRITE(obj.timestamp); }

    CTimestampIndexIteratorKey(unsigned int time) {
        timestamp = time;
    }

    CTimestampIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
    }
};

struct CTimestampIndexKey {
    unsigned int timestamp;
    uint256 blockHash;

    SERIALIZE_METHODS(CTimestampIndexKey, obj) { READWRITE(obj.timestamp, obj.blockHash); }

    CTimestampIndexKey(unsigned int time, uint256 hash) {
        timestamp = time;
        blockHash = hash;
    }

    CTimestampIndexKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
        blockHash.SetNull();
    }
};

struct CAddressUnspentKey {
    unsigned int type;
    uint256 hashBytes;
    uint256 txhash;
    unsigned int index;

    SERIALIZE_METHODS(CAddressUnspentKey, obj) {
        READWRITE(obj.type, obj.hashBytes, obj.txhash, obj.index);
    }

    CAddressUnspentKey(unsigned int addressType, uint256 addressHash, uint256 txid, unsigned int indexValue) {
        type = addressType;
        hashBytes = addressHash;
        txhash = txid;
        index = indexValue;
    }

    CAddressUnspentKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        txhash.SetNull();
        index = 0;
    }
};

struct CAddressUnspentValue {
    CAmount satoshis;
    CScript script;
    int blockHeight;

    SERIALIZE_METHODS(CAddressUnspentValue, obj)
    {
        READWRITE(obj.satoshis, obj.script, obj.blockHeight);
    }

    CAddressUnspentValue(CAmount sats, CScript scriptPubKey, int height) {
        satoshis = sats;
        script = scriptPubKey;
        blockHeight = height;
    }

    CAddressUnspentValue() {
        SetNull();
    }

    void SetNull() {
        satoshis = -1;
        script.clear();
        blockHeight = 0;
    }

    bool IsNull() const {
        return (satoshis == -1);
    }
};

struct CAddressIndexKey {
    unsigned int type;
    uint256 hashBytes;
    int blockHeight;
    unsigned int txindex;
    uint256 txhash;
    unsigned int index;
    bool spending;

    SERIALIZE_METHODS(CAddressIndexKey, obj) {READWRITE(obj.type, obj.hashBytes, obj.blockHeight, obj.txindex, obj.txhash, obj.index, obj.spending);}

    CAddressIndexKey(unsigned int addressType, uint256 addressHash, int height, int blockindex,
                     uint256 txid, unsigned int indexValue, bool isSpending) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
        txindex = blockindex;
        txhash = txid;
        index = indexValue;
        spending = isSpending;
    }

    CAddressIndexKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
        txindex = 0;
        txhash.SetNull();
        index = 0;
        spending = false;
    }

};

struct CAddressIndexIteratorKey {
    unsigned int type;
    uint256 hashBytes;

    SERIALIZE_METHODS(CAddressIndexIteratorKey, obj) {READWRITE(obj.type, obj.hashBytes);}

    CAddressIndexIteratorKey(unsigned int addressType, uint256 addressHash) {
        type = addressType;
        hashBytes = addressHash;
    }

    CAddressIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
    }
};

struct CAddressIndexIteratorHeightKey {
    unsigned int type;
    uint256 hashBytes;
    int blockHeight;

    SERIALIZE_METHODS(CAddressIndexIteratorHeightKey, obj) {READWRITE(obj.type, obj.hashBytes, obj.blockHeight);}

    CAddressIndexIteratorHeightKey(unsigned int addressType, uint256 addressHash, int height) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
    }

    CAddressIndexIteratorHeightKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
    }
};

#endif // BITCOIN_SPENTINDEX_H