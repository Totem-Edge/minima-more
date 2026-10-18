#pragma once
//
// MiniBuffer - small-buffer-optimised byte container used by MiniData.
//
// Purpose: most MiniData payloads are small (32-byte hashes/addresses/CoinIDs,
// up to 64-byte hashes). A std::vector heap-allocates for every one of those,
// which dominates the in-memory MMR/UTXO framing cost. MiniBuffer stores up to
// INLINE_CAP bytes inside the object and only falls back to the heap above that.
//
// It deliberately exposes a small vector-like API (data/size/empty/begin/end/
// operator[]/assign/resize/push_back/erase) plus an implicit conversion to
// std::vector<std::uint8_t> so existing callers that bind a std::vector keep
// compiling (materialising a copy only when they actually need one).
//
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <initializer_list>

namespace org {
namespace minima {
namespace objects {
namespace base {

class MiniBuffer {
public:
    using value_type = std::uint8_t;
    using iterator = std::uint8_t*;
    using const_iterator = const std::uint8_t*;

    static constexpr std::uint32_t INLINE_CAP = 32;

    MiniBuffer() = default;

    MiniBuffer(const std::uint8_t* zData, std::size_t zLen) { assign(zData, zLen); }
    MiniBuffer(const std::vector<std::uint8_t>& zVec) { assign(zVec.data(), zVec.size()); }
    MiniBuffer(std::initializer_list<std::uint8_t> zInit) { assign(zInit.begin(), zInit.size()); }

    MiniBuffer(const MiniBuffer& zOther) { assign(zOther.data(), zOther.size()); }
    MiniBuffer(MiniBuffer&& zOther) noexcept { moveFrom(zOther); }

    MiniBuffer& operator=(const MiniBuffer& zOther) {
        if (this != &zOther) {
            resize(0);
            assign(zOther.data(), zOther.size());
        }
        return *this;
    }
    MiniBuffer& operator=(MiniBuffer&& zOther) noexcept {
        if (this != &zOther) {
            destroy();
            moveFrom(zOther);
        }
        return *this;
    }

    ~MiniBuffer() { destroy(); }

    // --- read ---
    const std::uint8_t* data() const { return mHeap ? mHeap : mInline; }
    std::uint8_t*       data()       { return mHeap ? mHeap : mInline; }
    std::size_t size()  const { return mLen; }
    bool        empty() const { return mLen == 0; }

    std::uint8_t  operator[](std::size_t i) const { return data()[i]; }
    std::uint8_t& operator[](std::size_t i)       { return data()[i]; }

    const std::uint8_t* begin() const { return data(); }
    const std::uint8_t* end()   const { return data() + mLen; }
    std::uint8_t*       begin()       { return data(); }
    std::uint8_t*       end()         { return data() + mLen; }

    // --- mutation (used internally by MiniData) ---
    void clear() { mLen = 0; }

    void assign(std::size_t zLen, std::uint8_t zVal) {
        resize(zLen);
        if (zLen) std::memset(data(), zVal, zLen);
    }
    void assign(const std::uint8_t* zData, std::size_t zLen) {
        resize(zLen);
        if (zLen && zData) std::memcpy(data(), zData, zLen);
    }

    void resize(std::size_t zLen) {
        ensure(static_cast<std::uint32_t>(zLen));
        mLen = static_cast<std::uint32_t>(zLen);
    }

    void push_back(std::uint8_t zVal) {
        ensure(mLen + 1);
        data()[mLen++] = zVal;
    }

    // Erase the byte at the given iterator (pointer) position.
    void erase(const std::uint8_t* zIt) {
        const std::size_t idx = static_cast<std::size_t>(zIt - data());
        if (idx < mLen) {
            std::memmove(data() + idx, data() + idx + 1, mLen - idx - 1);
            --mLen;
        }
    }

    // --- compatibility ---
    // Materialise a std::vector copy on demand. Lets call sites that require a
    // real std::vector (const-ref bindings, vector-taking functions) compile.
    operator std::vector<std::uint8_t>() const {
        return std::vector<std::uint8_t>(data(), data() + mLen);
    }

    bool operator==(const MiniBuffer& zOther) const {
        return mLen == zOther.mLen &&
               (mLen == 0 || std::memcmp(data(), zOther.data(), mLen) == 0);
    }
    bool operator!=(const MiniBuffer& zOther) const { return !(*this == zOther); }

private:
    void ensure(std::uint32_t zNeeded) {
        if (zNeeded <= mCap) return;
        std::uint32_t newcap = zNeeded < 64 ? 64 : zNeeded * 2;
        std::uint8_t* p = new std::uint8_t[newcap];
        if (mLen) std::memcpy(p, data(), mLen);
        if (mHeap) delete[] mHeap;
        mHeap = p;
        mCap = newcap;
    }

    void destroy() {
        if (mHeap) {
            delete[] mHeap;
            mHeap = nullptr;
        }
        mLen = 0;
        mCap = INLINE_CAP;
    }

    void moveFrom(MiniBuffer& zOther) noexcept {
        if (zOther.mHeap) {
            mHeap = zOther.mHeap;
            mLen = zOther.mLen;
            mCap = zOther.mCap;
            zOther.mHeap = nullptr;
            zOther.mLen = 0;
            zOther.mCap = INLINE_CAP;
        } else {
            mLen = zOther.mLen;
            mCap = INLINE_CAP;
            if (mLen) std::memcpy(mInline, zOther.mInline, mLen);
            zOther.mLen = 0;
        }
    }

    std::uint8_t  mInline[INLINE_CAP];
    std::uint8_t* mHeap { nullptr };
    std::uint32_t mLen { 0 };
    std::uint32_t mCap { INLINE_CAP };
};

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org
