#pragma once
#include "sha256.h"
#include <vector>
#include <memory>

// -----------------------------------------------------------------------
// Hashable interface
// -----------------------------------------------------------------------
class Hashable {
public:
    virtual uint256_t getHash() const = 0;
    virtual ~Hashable() = default;
};

// -----------------------------------------------------------------------
// HashableMerkleTree
// -----------------------------------------------------------------------
class HashableMerkleTree {
private:
    std::vector<uint256_t> hashes;

    static uint256_t combineHashes(const uint256_t& a, const uint256_t& b) {
        std::vector<uint8_t> buf(64);
        for (int i = 0; i < 32; ++i) { buf[i] = a[i]; buf[32+i] = b[i]; }
        return SHA256::hashBytes(buf);
    }

    static uint256_t calcRoot(std::vector<uint256_t> level) {
        if (level.empty()) return zero256();
        while (level.size() > 1) {
            if (level.size() % 2 == 1) level.push_back(zero256());
            std::vector<uint256_t> next;
            for (size_t i = 0; i < level.size(); i += 2)
                next.push_back(combineHashes(level[i], level[i+1]));
            level = std::move(next);
        }
        return level[0];
    }

public:
    HashableMerkleTree() {}

    HashableMerkleTree(const std::vector<uint256_t>& h) : hashes(h) {}

    template<typename T>
    HashableMerkleTree(const std::vector<T>& items) {
        for (const auto& item : items) hashes.push_back(item.getHash());
    }

    template<typename T>
    HashableMerkleTree(const std::vector<std::shared_ptr<T>>& items) {
        for (const auto& item : items) hashes.push_back(item->getHash());
    }

    uint256_t calcMerkleRoot() const {
        return calcRoot(hashes);
    }

    const std::vector<uint256_t>& getHashes() const { return hashes; }
};
