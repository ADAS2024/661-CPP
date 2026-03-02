#pragma once
#include "sha256.h"
#include "transaction.h"
#include "merkle.h"
#include <vector>
#include <memory>
#include <optional>
#include <string>
#include <random>
#include <cassert>

// -----------------------------------------------------------------------
// BlockContents: holds a MerkleTree of Transactions
// -----------------------------------------------------------------------
class BlockContents {
private:
    std::vector<std::shared_ptr<Transaction>> txList;
    HashableMerkleTree merkleTree;

public:
    BlockContents() {}

    void setData(const std::vector<std::shared_ptr<Transaction>>& data) {
        txList = data;
        std::vector<uint256_t> hashes;
        for (const auto& tx : txList) hashes.push_back(tx->getHash());
        merkleTree = HashableMerkleTree(hashes);
    }

    const HashableMerkleTree& getData() const { return merkleTree; }
    const std::vector<std::shared_ptr<Transaction>>& getTxList() const { return txList; }

    uint256_t calcMerkleRoot() const { return merkleTree.calcMerkleRoot(); }
};

// -----------------------------------------------------------------------
// Block
// -----------------------------------------------------------------------
class Block {
private:
    double version;
    std::optional<uint256_t> prev_hash;
    uint256_t diff_target;
    uint256_t merkle_root;
    double timestamp;
    uint64_t nonce;

    std::shared_ptr<BlockContents> content;
    UTXOSet utxo_set;
    long double cumul_work;
    bool has_target;

    static thread_local std::mt19937_64 rng;

public:
    Block()
        : version(1.0), timestamp(1.0), nonce(0),
          cumul_work(0.0), has_target(false) {
        merkle_root = zero256();
    }

    std::shared_ptr<BlockContents> getContents() const { return content; }

    void setContents(const std::vector<std::shared_ptr<Transaction>>& data) {
        content = std::make_shared<BlockContents>();
        content->setData(data);
        merkle_root = content->calcMerkleRoot();
    }

    void setUTXOSet(const UTXOSet& u) { utxo_set = u; }
    const UTXOSet& getUTXOSet() const { return utxo_set; }

    void setTarget(const uint256_t& target) { diff_target = target; has_target = true; }
    const uint256_t& getTarget() const { return diff_target; }

    uint256_t getHash() const {
        std::string s;
        s += std::to_string(version);
        if (prev_hash.has_value()) {
            s += to_hex(*prev_hash);
        }
        if (has_target) s += to_hex(diff_target);
        s += to_hex(merkle_root);
        s += std::to_string(timestamp);
        s += std::to_string(nonce);
        return SHA256::hashBytes(std::vector<uint8_t>(s.begin(), s.end()));
    }

    void setPriorBlockHash(const uint256_t& h) { prev_hash = h; }
    std::optional<uint256_t> getPriorBlockHash() const { return prev_hash; }

    void mine(const uint256_t& tgt) {
        diff_target = tgt;
        has_target = true;
        // Start with random nonce in [0, target_as_u64 mod 2^64]
        nonce = rng() % 0xFFFFFFFFFFFFFFFFULL;
        while (true) {
            uint256_t h = getHash();
            if (h < diff_target) break;
            ++nonce;
        }
    }

    // Cumulative work cache
    long double getCumulWork() const { return cumul_work; }
    void setCumulWork(long double w) { cumul_work = w; }

    // validate(unspentOutputs, maxMint)
    // Returns an optional UTXOSet: nullopt = invalid, has_value = valid with new UTXO
    std::optional<UTXOSet> validate(const UTXOSet& unspentOutputs, double maxMint) const {
        assert(has_target);

        // Check POW
        if (getHash() >= diff_target) return std::nullopt;

        // No content: still valid
        if (!content) {
            return unspentOutputs;
        }

        const auto& txList = content->getTxList();
        if (txList.empty()) return unspentOutputs;

        // First tx must be coinbase
        auto& coinbase = txList[0];
        if (!coinbase->isCoinbase()) return std::nullopt;
        if (!coinbase->validateMint(maxMint)) return std::nullopt;

        UTXOSet newUTXO = unspentOutputs;

        // Validate non-coinbase txns
        for (size_t i = 1; i < txList.size(); ++i) {
            const auto& tx = txList[i];
            if (!tx->validate(newUTXO)) return std::nullopt;

            // Remove spent outputs
            for (const auto& inp : *tx->inputs) {
                UTXOKey key{inp.txHash, inp.txIdx};
                newUTXO.erase(key);
            }

            // Add new outputs
            for (size_t j = 0; j < tx->outputs.size(); ++j) {
                UTXOKey key{tx->getHash(), (int)j};
                newUTXO[key] = tx->outputs[j];
            }
        }

        // Add coinbase outputs
        for (size_t j = 0; j < coinbase->outputs.size(); ++j) {
            UTXOKey key{coinbase->getHash(), (int)j};
            newUTXO[key] = coinbase->outputs[j];
        }

        return newUTXO;
    }
};

thread_local std::mt19937_64 Block::rng(std::random_device{}());
