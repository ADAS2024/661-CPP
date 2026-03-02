#include "blockchain.h"
#include <iostream>
#include <cassert>
#include <sstream>

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
// makeTarget mirrors Python: int("F"*fCount, 16)
// "F"*fCount is a hex string of fCount chars, left-zero-padded to 64 chars
static uint256_t makeTarget(int fCount) {
    std::string hex(64, '0');
    for (int i = 64 - fCount; i < 64; ++i) hex[i] = 'f';
    return hexTo256(hex);
}

// -----------------------------------------------------------------------
// Test: HashableMerkleTree
// -----------------------------------------------------------------------
struct GivesHash {
    uint256_t hash;
    uint256_t getHash() const { return hash; }
};

static uint256_t fromBigInt(const std::string& decStr) {
    // Parse very large decimal numbers into uint256_t
    // We'll use a simple approach: treat as hex if it looks hex,
    // but for these test values they're decimal. Use Python-style approach.
    // Actually the test values in Python are just big integers.
    // Let's implement big decimal to bytes.
    std::vector<uint8_t> bytes(32, 0);
    // Simple long division approach
    std::string n = decStr;
    for (int i = 31; i >= 0; --i) {
        // Extract byte: n mod 256
        int rem = 0;
        std::string q;
        for (char c : n) {
            rem = rem * 10 + (c - '0');
            q += (char)('0' + rem / 256);
            rem %= 256;
        }
        bytes[i] = (uint8_t)rem;
        // Remove leading zeros from q
        size_t start = 0;
        while (start < q.size() - 1 && q[start] == '0') start++;
        n = q.substr(start);
        if (n == "0" && i > 0) { break; }
    }
    uint256_t r;
    for (int i = 0; i < 32; ++i) r[i] = bytes[i];
    return r;
}

void TestMerkle() {
    {
        // Single element test
        GivesHash g;
        g.hash = fromBigInt("106874969902263813231722716312951672277654786095989753245644957127312510061509");
        HashableMerkleTree tree({g.hash});
        uint256_t root = tree.calcMerkleRoot();
        std::string expected = "ec4916dd28fc4c10d78e287ca5d9cc51ee1ae73cbfde08c6b37324cbfaac8bc5";
        assert(to_hex(root) == expected && "Single element merkle root mismatch");
    }
    {
        // Three element test
        GivesHash g1, g2, g3;
        g1.hash = fromBigInt("106874969902263813231722716312951672277654786095989753245644957127312510061509");
        g2.hash = fromBigInt("66221123338548294768926909213040317907064779196821799240800307624498097778386");
        g3.hash = fromBigInt("98188062817386391176748233602659695679763360599522475501622752979264247167302");
        HashableMerkleTree tree({g1.hash, g2.hash, g3.hash});
        uint256_t root = tree.calcMerkleRoot();
        std::string expected = "ea670d796aa1f950025c4d9e7caf6b92a5c56ebeb37b95b072ca92bc99011c20";
        assert(to_hex(root) == expected && "Three element merkle root mismatch");
    }
    std::cout << "Passed Merkle Tests\n";
}

// -----------------------------------------------------------------------
// Test: Transaction
// -----------------------------------------------------------------------
void TestTransaction() {
    // Coinbase
    auto t0 = std::make_shared<Transaction>(std::nullopt,
        std::vector<Output>{Output([](const Satisfier&){ return true; }, 100)});
    assert(t0->isCoinbase());
    assert(t0->validateMint(100));
    assert(!t0->validateMint(50));

    // Build UTXOs for inputs
    uint256_t txHash1; txHash1.fill(0); txHash1[31] = 1;
    uint256_t txHash2; txHash2.fill(0); txHash2[31] = 2;

    ConstraintFn c1 = [](const Satisfier& x){ return (x[0]+x[1]) == 100; };
    ConstraintFn c2 = [](const Satisfier& x){ return (x[1]-x[0]) == 30; };

    Output out1(c1, 100);
    Output out2(c2, 30);

    // satisfier1 = [40, 60] -> 40+60=100 -> passes c1
    // satisfier2 = [40, 70] -> 70-40=30 -> passes c2
    Satisfier s1 = {Value(40), Value(60)};
    Satisfier s2 = {Value(40), Value(70)};

    Input inp1(txHash1, 0, s1);
    Input inp2(txHash2, 1, s2);

    UTXOSet utxoFalse = {
        {{txHash1, 0}, out1},
        {{txHash2, 1}, out1}   // inp2 references out1 but s2=[40,70] fails c1
    };
    UTXOSet utxoTrue = {
        {{txHash1, 0}, out1},
        {{txHash2, 1}, out2}
    };

    auto tx = std::make_shared<Transaction>(
        std::vector<Input>{inp1, inp2},
        std::vector<Output>{out1, out2},
        std::vector<uint8_t>{'D','C','B','A'}
    );

    assert(!tx->validate(utxoFalse) && "Should fail with wrong UTXO");
    assert(tx->validate(utxoTrue) && "Should pass with correct UTXO");

    // getOutput
    assert(tx->getOutput(0).amount == 100);

    std::cout << "Passed Transaction Tests\n";
}

// -----------------------------------------------------------------------
// Test: Block
// -----------------------------------------------------------------------
void TestBlock() {
    uint256_t genesisTarget = makeTarget(64);
    double maxMint = 100.0;

    // Block with no content: valid if POW is satisfied
    auto b0 = std::make_shared<Block>();
    b0->mine(genesisTarget);
    auto utxo0 = b0->validate(UTXOSet{}, maxMint);
    assert(utxo0.has_value() && "Block with no content should be valid");

    // Block with only coinbase
    auto t0 = std::make_shared<Transaction>(std::nullopt,
        std::vector<Output>{Output([](const Satisfier&){ return true; }, 100)});
    b0->setContents({t0});
    auto utxo0b = b0->validate(UTXOSet{}, maxMint);
    assert(utxo0b.has_value() && "Block with only coinbase should be valid");

    // Build a second block with a spending transaction
    uint256_t b1Target = makeTarget(63);

    // Create input referring to coinbase output
    uint256_t cbHash = t0->getHash();
    Satisfier s = {Value(1)};
    Input inp(cbHash, 0, {Value(1)});

    Output spendOut([](const Satisfier&){ return true; }, 100);
    auto t1 = std::make_shared<Transaction>(
        std::vector<Input>{inp},
        std::vector<Output>{spendOut}
    );

    auto b1 = std::make_shared<Block>();
    b1->setPriorBlockHash(b0->getHash());
    b1->setContents({t0, t1});
    b1->mine(b1Target);

    // Should fail with empty UTXO (t1 references t0 which is not in empty UTXO)
    auto bad = b1->validate(UTXOSet{}, maxMint);
    assert(!bad.has_value() && "Should fail: input not in empty UTXO");

    // Should succeed with the UTXO from b0
    auto good = b1->validate(*utxo0b, maxMint);
    assert(good.has_value() && "Should succeed with proper UTXO");

    std::cout << "Passed Block.validate() Tests\n";
}

// -----------------------------------------------------------------------
// Test: Blockchain
// -----------------------------------------------------------------------
void TestBlockchain() {
    uint256_t genesisTarget = makeTarget(64);
    double maxMint = 100.0;

    Blockchain bc(genesisTarget, maxMint);
    assert(bc.getGenesisBlock() != nullptr && "Genesis block should exist");

    auto coinbase = std::make_shared<Transaction>(std::nullopt,
        std::vector<Output>{Output([](const Satisfier&){ return true; }, 100)});

    // Extend with genesis (no content)
    bc.extend(bc.getGenesisBlock());

    // Now set contents and re-extend
    bc.getGenesisBlock()->setContents({coinbase});
    bc.extend(bc.getGenesisBlock());

    assert(bc.getTip() == bc.getGenesisBlock() && "Only genesis in chain");

    // Build b1 on top of genesis
    auto b1 = std::make_shared<Block>();
    b1->setPriorBlockHash(bc.getGenesisBlock()->getHash());

    // Make a tx spending coinbase output
    uint256_t cbHash = coinbase->getHash();
    Input inp(cbHash, 0, {Value(1)});
    auto spendTx = std::make_shared<Transaction>(
        std::vector<Input>{inp},
        std::vector<Output>{Output([](const Satisfier&){ return true; }, 100)}
    );
    auto cb2 = std::make_shared<Transaction>(std::nullopt,
        std::vector<Output>{Output([](const Satisfier&){ return true; }, 100)});
    b1->setContents({cb2, spendTx});
    b1->mine(makeTarget(63));

    bool ok = bc.extend(b1);
    assert(ok && "b1 should extend successfully");
    assert(bc.getTip() == b1 && "b1 should be tip");

    // Build b2 on top of b1
    auto b2 = std::make_shared<Block>();
    b2->setPriorBlockHash(b1->getHash());
    b2->setContents({cb2});
    b2->mine(makeTarget(62));

    ok = bc.extend(b2);
    assert(ok && "b2 should extend");
    assert(bc.getTip() == b2 && "b2 should be tip");

    // Build fork block on b1 with more work (target(59) has more work than target(62))
    auto forkBlk = std::make_shared<Block>();
    forkBlk->setPriorBlockHash(b1->getHash());
    forkBlk->setContents({cb2});
    forkBlk->mine(makeTarget(59));

    ok = bc.extend(forkBlk);
    assert(ok && "fork block should extend");
    // Fork should now be the tip due to more work
    assert(bc.getTip() == forkBlk && "fork block should be new tip");

    // Cumulative work checks
    double gWork = bc.getWork(genesisTarget);
    assert(gWork == 1.0 && "Genesis work should be 1");

    double b1CumulWork = bc.getCumulativeWork(b1->getHash());
    double expected_b1 = div256(genesisTarget, genesisTarget) + div256(genesisTarget, makeTarget(63));
    assert(std::abs(b1CumulWork - expected_b1) < 1e-9 && "b1 cumulative work mismatch");

    // getBlocksAtHeight
    auto height2Blocks = bc.getBlocksAtHeight(2);
    assert(height2Blocks.size() == 2 && "Should have 2 blocks at height 2");
    bool hasB2 = false, hasFork = false;
    for (auto& blk : height2Blocks) {
        if (blk == b2) hasB2 = true;
        if (blk == forkBlk) hasFork = true;
    }
    assert(hasB2 && hasFork && "Both b2 and forkBlk should be at height 2");

    auto height1Blocks = bc.getBlocksAtHeight(1);
    assert(height1Blocks.size() == 1 && height1Blocks[0] == b1 && "Only b1 at height 1");

    // Invalid fork block should not be added
    // Make a transaction that references something not in UTXO
    uint256_t fakeHash; fakeHash.fill(0xAB);
    Input badInp(fakeHash, 0, {});
    auto badTx = std::make_shared<Transaction>(
        std::vector<Input>{badInp},
        std::vector<Output>{Output(nullptr, 50)}
    );
    auto invalidFork = std::make_shared<Block>();
    invalidFork->setPriorBlockHash(forkBlk->getHash());
    invalidFork->setContents({cb2, badTx});
    invalidFork->mine(makeTarget(62));

    bool badOk = bc.extend(invalidFork);
    assert(!badOk && "Invalid fork block should not be added");

    // Verify it's not in either list
    const auto& bl = bc.getBlocklist();
    const auto& fl = bc.getForklist();
    assert(bl.find(invalidFork->getHash()) == bl.end() && "Not in blocklist");
    assert(fl.find(invalidFork->getHash()) == fl.end() && "Not in forklist");

    std::cout << "Passed Blockchain Tests\n";
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------
int main() {
    std::cout << "Running blockchain tests...\n";
    TestMerkle();
    TestTransaction();
    TestBlock();
    TestBlockchain();
    std::cout << "\nAll tests passed!\n";
    return 0;
}
