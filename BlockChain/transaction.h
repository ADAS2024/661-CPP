#pragma once
#include "sha256.h"
#include <functional>
#include <vector>
#include <stdexcept>
#include <map>
#include <optional>
#include <cstring>
#include <string>

// -----------------------------------------------------------------------
// Value: generic type for satisfier lists (numeric or string)
// -----------------------------------------------------------------------
struct Value {
    enum Type { NUMERIC, STRING_TYPE } type;
    double dval;
    std::string sval;

    Value() : type(NUMERIC), dval(0) {}
    Value(int v) : type(NUMERIC), dval((double)v) {}
    Value(double v) : type(NUMERIC), dval(v) {}
    Value(const std::string& v) : type(STRING_TYPE), sval(v), dval(0) {}

    double asDouble() const { return dval; }
    std::string asString() const { return sval; }

    Value operator+(const Value& o) const { return Value(dval + o.dval); }
    Value operator-(const Value& o) const { return Value(dval - o.dval); }
    bool operator==(double d) const { return dval == d; }
    bool operator==(int d) const { return dval == (double)d; }
    bool operator==(const Value& o) const {
        if (type != o.type) return false;
        return (type == NUMERIC) ? dval == o.dval : sval == o.sval;
    }
};

using Satisfier = std::vector<Value>;
using ConstraintFn = std::function<bool(const Satisfier&)>;

// -----------------------------------------------------------------------
// Output
// -----------------------------------------------------------------------
class Output {
public:
    ConstraintFn constraint;
    double amount;

    Output() : constraint(nullptr), amount(0) {}
    Output(ConstraintFn c, double a) : constraint(c), amount(a) {}

    bool checkConstraint(const Satisfier& satisfier) const {
        if (!constraint) return true;
        try { return constraint(satisfier); } catch (...) { return false; }
    }
};

// -----------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------
class Input {
public:
    uint256_t txHash;
    int txIdx;
    Satisfier satisfier;

    Input(const uint256_t& h, int idx, const Satisfier& s)
        : txHash(h), txIdx(idx), satisfier(s) {}
};

// -----------------------------------------------------------------------
// UTXO
// -----------------------------------------------------------------------
struct UTXOKey {
    uint256_t txHash;
    int txIdx;
    bool operator<(const UTXOKey& other) const {
        if (txHash != other.txHash) return txHash < other.txHash;
        return txIdx < other.txIdx;
    }
};

using UTXOSet = std::map<UTXOKey, Output>;

// -----------------------------------------------------------------------
// Transaction
// -----------------------------------------------------------------------
class Transaction {
private:
    uint256_t hash_val;

    static void pushU32(std::vector<uint8_t>& buf, uint32_t v) {
        buf.push_back((v>>24)&0xff); buf.push_back((v>>16)&0xff);
        buf.push_back((v>>8)&0xff);  buf.push_back(v&0xff);
    }
    static void pushDouble(std::vector<uint8_t>& buf, double d) {
        uint64_t bits = 0; memcpy(&bits, &d, 8);
        for (int i = 7; i >= 0; --i) buf.push_back((bits>>(i*8))&0xff);
    }

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        buf.push_back(inputs.has_value() ? 1 : 0);
        if (inputs.has_value()) {
            pushU32(buf, (uint32_t)inputs->size());
            for (const auto& inp : *inputs) {
                for (auto b : inp.txHash) buf.push_back(b);
                pushU32(buf, (uint32_t)inp.txIdx);
                pushU32(buf, (uint32_t)inp.satisfier.size());
                for (const auto& v : inp.satisfier) {
                    buf.push_back((uint8_t)v.type);
                    if (v.type == Value::NUMERIC) {
                        pushDouble(buf, v.dval);
                    } else {
                        pushU32(buf, (uint32_t)v.sval.size());
                        for (char c : v.sval) buf.push_back((uint8_t)c);
                    }
                }
            }
        }
        pushU32(buf, (uint32_t)outputs.size());
        for (const auto& out : outputs) pushDouble(buf, out.amount);
        for (auto b : data) buf.push_back(b);
        return buf;
    }

public:
    std::optional<std::vector<Input>> inputs;
    std::vector<Output> outputs;
    std::vector<uint8_t> data;

    Transaction() {}

    // Coinbase constructor
    Transaction(std::nullopt_t, std::vector<Output> outs, std::vector<uint8_t> d = {})
        : inputs(std::nullopt), outputs(std::move(outs)), data(std::move(d)) {
        hash_val = calcHash();
    }

    // Regular transaction constructor
    Transaction(std::vector<Input> ins, std::vector<Output> outs, std::vector<uint8_t> d = {})
        : inputs(std::move(ins)), outputs(std::move(outs)), data(std::move(d)) {
        hash_val = calcHash();
    }

    uint256_t calcHash() const {
        auto buf = serialize();
        return SHA256::hashBytes(buf);
    }

    uint256_t getHash() const { return hash_val; }

    const std::optional<std::vector<Input>>& getInputs() const { return inputs; }

    const Output& getOutput(size_t n) const {
        if (n >= outputs.size()) throw std::out_of_range("Output index out of range");
        return outputs[n];
    }

    bool isCoinbase() const { return !inputs.has_value(); }

    bool validateMint(double maxCoinsToCreate) const {
        double sum = 0;
        for (const auto& out : outputs) sum += out.amount;
        return sum <= maxCoinsToCreate;
    }

    bool validate(const UTXOSet& utxo) const {
        if (!inputs.has_value()) return false;
        double totalIn = 0, totalOut = 0;
        for (const auto& inp : *inputs) {
            UTXOKey key{inp.txHash, inp.txIdx};
            auto it = utxo.find(key);
            if (it == utxo.end()) return false;
            if (!it->second.checkConstraint(inp.satisfier)) return false;
            totalIn += it->second.amount;
        }
        for (const auto& out : outputs) totalOut += out.amount;
        return totalIn >= totalOut;
    }
};
