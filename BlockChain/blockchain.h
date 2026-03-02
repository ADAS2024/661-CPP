#pragma once
#include "block.h"
#include <map>
#include <memory>
#include <vector>
#include <limits>

// -----------------------------------------------------------------------
// Blockchain
// -----------------------------------------------------------------------
class Blockchain {
private:
    uint256_t genesisTarget;
    double maxMintCoinsPerTx;

    // blocklist: main chain
    std::map<uint256_t, std::shared_ptr<Block>> blocklist;
    // forklist: fork chains
    std::map<uint256_t, std::shared_ptr<Block>> forklist;

    std::shared_ptr<Block> genesisBlock;

    std::shared_ptr<Block> createGenesisBlock(const uint256_t& target) {
        auto b = std::make_shared<Block>();
        b->setTarget(target);
        b->mine(target);
        return b;
    }

public:
    Blockchain(const uint256_t& gTarget, double maxMint)
        : genesisTarget(gTarget), maxMintCoinsPerTx(maxMint) {
        genesisBlock = createGenesisBlock(gTarget);
    }

    std::shared_ptr<Block> getGenesisBlock() const { return genesisBlock; }

    // getWork: genesis_target / block_target (as double)
    long double getWork(const uint256_t& target) const {
        return div256(genesisTarget, target);
    }

    std::shared_ptr<Block> getTip() const {
        std::shared_ptr<Block> tip = nullptr;
        long double maxWork = 0.0L;

        for (const auto& kv : blocklist) {
            long double w = getCumulativeWorkInternal(kv.first);
            if (w > maxWork) { maxWork = w; tip = kv.second; }
        }
        for (const auto& kv : forklist) {
            long double w = getCumulativeWorkInternal(kv.first);
            if (w > maxWork) { maxWork = w; tip = kv.second; }
        }
        return tip;
    }

    long double getCumulativeWorkInternal(const uint256_t& blkHash) const {
        long double work = 0.0L;
        uint256_t current = blkHash;
        while (true) {
            std::shared_ptr<Block> blk;
            auto it = blocklist.find(current);
            if (it != blocklist.end()) blk = it->second;
            else {
                auto it2 = forklist.find(current);
                if (it2 != forklist.end()) blk = it2->second;
                else break;
            }
            work += getWork(blk->getTarget());
            auto prev = blk->getPriorBlockHash();
            if (!prev.has_value()) break;
            current = *prev;
        }
        return work;
    }

    // getCumulativeWork: returns -1 if block not in chain
    long double getCumulativeWork(const uint256_t& blkHash) const {
        bool inMain = blocklist.count(blkHash) > 0;
        bool inFork = forklist.count(blkHash) > 0;
        if (!inMain && !inFork) return -1.0L;
        return getCumulativeWorkInternal(blkHash);
    }

    // getBlocksAtHeight: height 0 = genesis
    std::vector<std::shared_ptr<Block>> getBlocksAtHeight(int height) const {
        std::vector<std::shared_ptr<Block>> result;

        // Walk all blocks and compute their height
        auto getHeight = [&](const uint256_t& h) -> int {
            int d = 0;
            uint256_t current = h;
            while (true) {
                std::shared_ptr<Block> blk;
                auto it = blocklist.find(current);
                if (it != blocklist.end()) blk = it->second;
                else {
                    auto it2 = forklist.find(current);
                    if (it2 != forklist.end()) blk = it2->second;
                    else break;
                }
                auto prev = blk->getPriorBlockHash();
                if (!prev.has_value()) break;
                ++d;
                current = *prev;
            }
            return d;
        };

        for (const auto& kv : blocklist) {
            if (getHeight(kv.first) == height) result.push_back(kv.second);
        }
        for (const auto& kv : forklist) {
            if (getHeight(kv.first) == height) result.push_back(kv.second);
        }
        return result;
    }

    // extend: add block to chain. Returns false if invalid.
    bool extend(std::shared_ptr<Block> block) {
        auto prevHashOpt = block->getPriorBlockHash();

        // No previous hash = genesis block placement
        if (!prevHashOpt.has_value()) {
            if (blocklist.empty()) {
                // Validate genesis
                auto utxo = block->validate(UTXOSet{}, maxMintCoinsPerTx);
                if (!utxo.has_value()) {
                    // For empty-content genesis blocks still allow
                    block->setUTXOSet(UTXOSet{});
                    blocklist[block->getHash()] = block;
                    return true;
                }
                block->setUTXOSet(*utxo);
                blocklist[block->getHash()] = block;
                return true;
            } else {
                // Replace genesis block (updated with content)
                // Find and remove old genesis
                uint256_t oldHash;
                bool found = false;
                for (const auto& kv : blocklist) {
                    if (!kv.second->getPriorBlockHash().has_value()) {
                        oldHash = kv.first;
                        found = true;
                        break;
                    }
                }
                if (found) blocklist.erase(oldHash);
                auto utxo = block->validate(UTXOSet{}, maxMintCoinsPerTx);
                UTXOSet u = utxo.has_value() ? *utxo : UTXOSet{};
                block->setUTXOSet(u);
                blocklist[block->getHash()] = block;
                return true;
            }
        }

        const uint256_t& prevHash = *prevHashOpt;

        // Parent must exist
        std::shared_ptr<Block> parent;
        auto it = blocklist.find(prevHash);
        if (it != blocklist.end()) parent = it->second;
        else {
            auto it2 = forklist.find(prevHash);
            if (it2 != forklist.end()) parent = it2->second;
            else return false;
        }

        // Validate block
        auto utxoOpt = block->validate(parent->getUTXOSet(), maxMintCoinsPerTx);
        if (!utxoOpt.has_value()) return false;

        block->setUTXOSet(*utxoOpt);

        // Determine if this extends the main chain tip or is a fork
        auto tip = getTip();
        if (tip && parent->getHash() == tip->getHash()) {
            blocklist[block->getHash()] = block;
        } else {
            forklist[block->getHash()] = block;
        }
        return true;
    }

    // Access to internal maps (for testing)
    const std::map<uint256_t, std::shared_ptr<Block>>& getBlocklist() const { return blocklist; }
    const std::map<uint256_t, std::shared_ptr<Block>>& getForklist() const { return forklist; }
};
