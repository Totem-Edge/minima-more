#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace org {
namespace minima {
namespace utils {

class FastByteArrayStream {
public:
    // 256k max initial size + 256K when needed
    static constexpr int SIZE_INCREASE = 256 * 1024;

    // Construct with an intended total size; initial allocation capped to SIZE_INCREASE
    explicit FastByteArrayStream(int totalSize);

    // Non-copyable, movable
    FastByteArrayStream(const FastByteArrayStream&) = delete;
    FastByteArrayStream& operator=(const FastByteArrayStream&) = delete;
    FastByteArrayStream(FastByteArrayStream&&) noexcept = default;
    FastByteArrayStream& operator=(FastByteArrayStream&&) noexcept = default;

    ~FastByteArrayStream() = default;

    // Write data from a byte vector with offset and length.
    // Semantics:
    // - If off < 0, returns immediately (no-op), matching the Java code.
    // - Otherwise validates bounds similar to System.arraycopy (throws on invalid).
    void writeData(const std::vector<uint8_t>& b, int off, int len);

    // Current number of valid bytes written.
    std::size_t size() const noexcept;

    // Reset the stream (logical size becomes 0, capacity unchanged).
    void reset() noexcept;

    // Return a copy of the valid data region [0, size()).
    std::vector<uint8_t> toByteArray() const;

    // Access to internal buffer pointer and writable size (count).
    // Note: Only the first size() bytes are valid data.
    const uint8_t* data() const noexcept;

private:
    static int getRequiredSize(int total);

    void ensureCapacityForAppend(std::size_t len);

    std::vector<uint8_t> m_buf; // allocated buffer; size() equals allocated capacity
    std::size_t m_count{0};     // number of bytes written
};

} // namespace utils
} // namespace minima
} // namespace org