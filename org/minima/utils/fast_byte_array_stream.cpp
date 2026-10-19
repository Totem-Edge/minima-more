#include "org/minima/utils/fast_byte_array_stream.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace org {
namespace minima {
namespace utils {

int FastByteArrayStream::getRequiredSize(int total) {
    if (total < SIZE_INCREASE) {
        return total;
    }
    return SIZE_INCREASE;
}

FastByteArrayStream::FastByteArrayStream(int totalSize) {
    int init = getRequiredSize(totalSize);
    if (init < 0) {
        init = 0;
    }
    m_buf.resize(static_cast<std::size_t>(init));
    m_count = 0;
}

void FastByteArrayStream::ensureCapacityForAppend(std::size_t len) {
    std::size_t csize = m_buf.size();
    std::size_t required = m_count + len;

    if (required > csize) {
        // How much bigger to make it..
        std::size_t increase = static_cast<std::size_t>(SIZE_INCREASE);
        if (len > increase) {
            increase = len;
        }

        // Make it bigger
        try {
            m_buf.resize(csize + increase);
        } catch (const std::bad_alloc&) {
            // Log similar to MinimaLogger.log and halt immediately
            std::fprintf(
                stderr,
                "[!] SERIOUS OUT OF MEMORY ERROR at FastByteArrayStream"
                " currentsize:%zu required:%zu increase:%zu count:%zu len:%zu\n",
                csize,
                required,
                increase,
                m_count,
                len
            );
            std::fflush(stderr);
            // Propagate instead of killing the whole node: the caller can then
            // reject the offending message/peer (was std::_Exit(0)).
            throw std::bad_alloc();
        }
    }
}

void FastByteArrayStream::writeData(const std::vector<uint8_t>& b, int off, int len) {
    // Check valid
    if (off < 0) {
        // Match Java behavior: silently return if off < 0
        return;
    }

    // Emulate System.arraycopy bounds checks for robustness
    if (len < 0) {
        throw std::out_of_range("writeData length < 0");
    }
    if (off > static_cast<int>(b.size())) {
        throw std::out_of_range("writeData offset > source length");
    }
    if (static_cast<std::size_t>(off) + static_cast<std::size_t>(len) > b.size()) {
        throw std::out_of_range("writeData offset+length exceeds source length");
    }

    // Ensure capacity
    ensureCapacityForAppend(static_cast<std::size_t>(len));

    // Now copy data
    if (len > 0) {
        std::memcpy(m_buf.data() + m_count, b.data() + static_cast<std::size_t>(off), static_cast<std::size_t>(len));
        // Increment counter
        m_count += static_cast<std::size_t>(len);
    }
}

std::size_t FastByteArrayStream::size() const noexcept {
    return m_count;
}

void FastByteArrayStream::reset() noexcept {
    m_count = 0;
}

std::vector<uint8_t> FastByteArrayStream::toByteArray() const {
    return std::vector<uint8_t>(m_buf.begin(), m_buf.begin() + static_cast<std::ptrdiff_t>(m_count));
}

const uint8_t* FastByteArrayStream::data() const noexcept {
    return m_buf.data();
}

} // namespace utils
} // namespace minima
} // namespace org