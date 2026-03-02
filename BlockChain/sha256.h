#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// SHA-256 implementation
class SHA256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;
    using Digest = std::array<uint8_t, DIGEST_SIZE>;

    SHA256() { reset(); }

    void reset() {
        state[0] = 0x6a09e667;
        state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372;
        state[3] = 0xa54ff53a;
        state[4] = 0x510e527f;
        state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab;
        state[7] = 0x5be0cd19;
        bitcount = 0;
        bufferLen = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buffer[bufferLen++] = data[i];
            if (bufferLen == 64) {
                transform();
                bufferLen = 0;
                bitcount += 512;
            }
        }
    }

    void update(const std::vector<uint8_t>& data) {
        update(data.data(), data.size());
    }

    void update(const std::string& s) {
        update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    Digest finalize() {
        bitcount += bufferLen * 8;
        buffer[bufferLen++] = 0x80;
        if (bufferLen > 56) {
            while (bufferLen < 64) buffer[bufferLen++] = 0;
            transform();
            bufferLen = 0;
        }
        while (bufferLen < 56) buffer[bufferLen++] = 0;
        for (int i = 7; i >= 0; --i) buffer[bufferLen++] = (bitcount >> (i * 8)) & 0xff;
        transform();

        Digest digest;
        for (int i = 0; i < 8; ++i) {
            digest[i*4+0] = (state[i] >> 24) & 0xff;
            digest[i*4+1] = (state[i] >> 16) & 0xff;
            digest[i*4+2] = (state[i] >>  8) & 0xff;
            digest[i*4+3] = (state[i]      ) & 0xff;
        }
        return digest;
    }

    static Digest hash(const uint8_t* data, size_t len) {
        SHA256 h; h.update(data, len); return h.finalize();
    }
    static Digest hash(const std::vector<uint8_t>& data) {
        return hash(data.data(), data.size());
    }
    static Digest hash(const std::string& s) {
        return hash(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    // Convert digest to big-endian 256-bit integer stored as 32-byte array
    // We represent large integers as std::array<uint8_t,32> (big endian)
    static std::array<uint8_t,32> hashBytes(const std::vector<uint8_t>& data) {
        SHA256 h; h.update(data); auto d = h.finalize();
        std::array<uint8_t,32> r; for(int i=0;i<32;i++) r[i]=d[i]; return r;
    }

private:
    uint32_t state[8];
    uint8_t buffer[64];
    size_t bufferLen;
    uint64_t bitcount;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32-n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x&y)^(~x&z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x&y)^(x&z)^(y&z); }
    static uint32_t sig0(uint32_t x) { return rotr(x,2)^rotr(x,13)^rotr(x,22); }
    static uint32_t sig1(uint32_t x) { return rotr(x,6)^rotr(x,11)^rotr(x,25); }
    static uint32_t gam0(uint32_t x) { return rotr(x,7)^rotr(x,18)^(x>>3); }
    static uint32_t gam1(uint32_t x) { return rotr(x,17)^rotr(x,19)^(x>>10); }

    void transform() {
        static const uint32_t K[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        uint32_t W[64];
        for(int i=0;i<16;i++) W[i]=(buffer[i*4]<<24)|(buffer[i*4+1]<<16)|(buffer[i*4+2]<<8)|buffer[i*4+3];
        for(int i=16;i<64;i++) W[i]=gam1(W[i-2])+W[i-7]+gam0(W[i-15])+W[i-16];
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for(int i=0;i<64;i++){
            uint32_t t1=h+sig1(e)+ch(e,f,g)+K[i]+W[i];
            uint32_t t2=sig0(a)+maj(a,b,c);
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
        state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    }
};

// Big integer as 32-byte big-endian array
using uint256_t = std::array<uint8_t, 32>;

// Comparison operators for uint256_t
inline bool operator<(const uint256_t& a, const uint256_t& b) {
    for(int i=0;i<32;i++) { if(a[i]<b[i]) return true; if(a[i]>b[i]) return false; }
    return false;
}
inline bool operator>=(const uint256_t& a, const uint256_t& b) { return !(a<b); }
inline bool operator>(const uint256_t& a, const uint256_t& b) { return b<a; }
inline bool operator<=(const uint256_t& a, const uint256_t& b) { return !(b<a); }
inline bool operator==(const uint256_t& a, const uint256_t& b) {
    for(int i=0;i<32;i++) if(a[i]!=b[i]) return false; return true;
}
inline bool operator!=(const uint256_t& a, const uint256_t& b) { return !(a==b); }

// Zero value
inline uint256_t zero256() { uint256_t z; z.fill(0); return z; }

// Max value (all F's)
inline uint256_t max256() { uint256_t m; m.fill(0xff); return m; }

// Divide two uint256_t values using 128-bit intermediate to preserve precision
// Returns a/b as a long double
inline long double div256(const uint256_t& a, const uint256_t& b) {
    // Find the number of leading zero bytes in each to normalize
    // We want high precision for targets that are close to max256.
    // Strategy: find leading nonzero bytes and use __uint128_t for the ratio
    // when the values fit, otherwise fall back to double.
    
    // Find effective lengths
    int aStart = 0, bStart = 0;
    while (aStart < 31 && a[aStart] == 0) ++aStart;
    while (bStart < 31 && b[bStart] == 0) ++bStart;
    
    // Use __uint128_t for up to 16 bytes precision
    __uint128_t da = 0, db = 0;
    // Use top 16 significant bytes of each
    int aTake = std::min(32 - aStart, 16);
    int bTake = std::min(32 - bStart, 16);
    
    for (int i = aStart; i < aStart + aTake && i < 32; ++i) { da = (da << 8) | a[i]; }
    for (int i = bStart; i < bStart + bTake && i < 32; ++i) { db = (db << 8) | b[i]; }
    
    // Adjust for different positions
    int aShift = (32 - aStart - aTake); // remaining bytes after taken
    int bShift = (32 - bStart - bTake);
    
    if (db == 0) return 0.0L;
    
    // Compute ratio with exponent adjustment
    // a = da * 256^aShift, b = db * 256^bShift
    // a/b = (da/db) * 256^(aShift-bShift)
    long double ratio = (long double)da / (long double)db;
    int exp = aShift - bShift;
    if (exp > 0) {
        for (int i = 0; i < exp; ++i) ratio *= 256.0L;
    } else {
        for (int i = 0; i < -exp; ++i) ratio /= 256.0L;
    }
    return ratio;
}

// Hex string to uint256_t
inline uint256_t hexTo256(const std::string& hex) {
    uint256_t r; r.fill(0);
    std::string h = hex;
    while(h.size() < 64) h = "0" + h;
    for(int i=0;i<32;i++) {
        r[i] = (uint8_t)std::stoul(h.substr(i*2,2), nullptr, 16);
    }
    return r;
}

inline std::string to_hex(const uint256_t& v) {
    static const char* hex = "0123456789abcdef";
    std::string s; s.reserve(64);
    for(auto b : v) { s += hex[b>>4]; s += hex[b&0xf]; }
    return s;
}
