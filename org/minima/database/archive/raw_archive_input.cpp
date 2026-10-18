#include "org/minima/database/archive/raw_archive_input.hpp"

#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <cstring>

#include <zlib.h>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace database {
namespace archive {

namespace {

// A streambuf that inflates gzip-compressed data from an std::ifstream.
class GzipStreamBuf : public std::streambuf {
public:
    GzipStreamBuf(const std::filesystem::path& path, std::size_t inbuf_size, std::size_t outbuf_size)
        : m_file(path, std::ios::in | std::ios::binary),
          m_inbuf(inbuf_size ? inbuf_size : 65536),
          m_outbuf(outbuf_size ? outbuf_size : 65536) {
        if (!m_file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path.string());
        }
        std::memset(&m_zstrm, 0, sizeof(m_zstrm));
        int ret = inflateInit2(&m_zstrm, 16 + MAX_WBITS); // 16 + MAX_WBITS -> gzip decoding
        if (ret != Z_OK) {
            throw std::runtime_error("inflateInit2 failed for gzip stream");
        }
        setg(m_outbuf.data(), m_outbuf.data(), m_outbuf.data()); // empty buffer
    }

    ~GzipStreamBuf() override {
        inflateEnd(&m_zstrm);
        // std::ifstream will close automatically
    }

protected:
    int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        if (m_finished) {
            return traits_type::eof();
        }

        m_zstrm.next_out = reinterpret_cast<Bytef*>(m_outbuf.data());
        m_zstrm.avail_out = static_cast<uInt>(m_outbuf.size());

        while (m_zstrm.avail_out > 0 && !m_finished) {
            if (m_zstrm.avail_in == 0 && !m_in_eof) {
                m_file.read(reinterpret_cast<char*>(m_inbuf.data()), static_cast<std::streamsize>(m_inbuf.size()));
                std::streamsize got = m_file.gcount();
                if (got <= 0) {
                    m_in_eof = true;
                    // No more input available; let inflate consume what's left (if any)
                } else {
                    m_zstrm.next_in = reinterpret_cast<Bytef*>(m_inbuf.data());
                    m_zstrm.avail_in = static_cast<uInt>(got);
                }
            }

            int ret = inflate(&m_zstrm, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                m_finished = true;
                break;
            }
            if (ret != Z_OK) {
                // On Z_BUF_ERROR with no output produced and EOF, treat as finished
                if (ret == Z_BUF_ERROR && m_in_eof && m_zstrm.avail_out != m_outbuf.size()) {
                    break;
                }
                return traits_type::eof();
            }

            // If no input and no output progress but EOF, break
            if (m_in_eof && m_zstrm.avail_out == m_outbuf.size()) {
                m_finished = true;
                break;
            }
        }

        std::size_t produced = m_outbuf.size() - m_zstrm.avail_out;
        if (produced == 0) {
            return traits_type::eof();
        }

        char* base = m_outbuf.data();
        setg(base, base, base + produced);
        return traits_type::to_int_type(*gptr());
    }

private:
    std::ifstream m_file;
    z_stream m_zstrm{};
    std::vector<unsigned char> m_inbuf;
    std::vector<char> m_outbuf;
    bool m_in_eof {false};
    bool m_finished {false};
};

// A simple std::istream wrapper around GzipStreamBuf
class GzipIStream : public std::istream {
public:
    GzipIStream(const std::filesystem::path& path, std::size_t inbuf_size, std::size_t outbuf_size)
        : std::istream(nullptr)
        , m_buf(path, inbuf_size, outbuf_size) {
        rdbuf(&m_buf);
    }

private:
    GzipStreamBuf m_buf;
};

} // anonymous namespace

RawArchiveInput::RawArchiveInput(const std::filesystem::path& zFile)
    : mFile(zFile) {
}

RawArchiveInput::~RawArchiveInput() = default;
RawArchiveInput::RawArchiveInput(RawArchiveInput&&) noexcept = default;
RawArchiveInput& RawArchiveInput::operator=(RawArchiveInput&&) noexcept = default;

void RawArchiveInput::connect() {
    // Create the decompressed input stream over the gzip file
    mDataIn = std::make_unique<GzipIStream>(mFile, 65536, 65536);

    // Is there a cascade
    org::minima::objects::base::MiniByte hasCasc = org::minima::objects::base::MiniByte::ReadFromStream(*mDataIn);
    if (hasCasc.isTrue()) {
        org::minima::utils::MinimaLogger::log("Cascade found in RAW archive..");
        auto casc = org::minima::database::cascade::Cascade::ReadFromStream(*mDataIn);
        mCascade = std::move(casc);
    } else {
        mCascade.reset();
    }

    // Now how many blocks in total
    mTotalFound = org::minima::objects::base::MiniNumber::ReadFromStream(*mDataIn).getAsInt();
    mTotalAdded = 0;

    org::minima::utils::MinimaLogger::log("Blocks found in RAW Archive : " + std::to_string(mTotalFound));
}

void RawArchiveInput::stop() {
    // Close in reverse dependency order by resetting unique_ptrs
    mDataIn.reset();
    // Keep mCascade as it may be needed by clients after stop(), mirroring Java holding state.
}

const org::minima::database::cascade::Cascade* RawArchiveInput::getCascade() const {
    return mCascade.get();
}

std::unique_ptr<org::minima::objects::IBD> RawArchiveInput::getNextIBD() {
    using org::minima::objects::IBD;
    using org::minima::objects::TxBlock;

    auto ret = std::make_unique<IBD>();

    if (mTotalAdded < mTotalFound) {
        // Add the cascade on the first call if present
        if (mTotalAdded == 0 && mCascade) {
            ret->setCascade(*mCascade);
        }

        // Try and load up to 256 blocks
        for (int i = 0; i < 256; ++i) {
            // Load a block
            std::unique_ptr<TxBlock> upblock = TxBlock::ReadFromStream(*mDataIn);

            // Add to IBD (convert unique_ptr -> shared_ptr)
            std::shared_ptr<TxBlock> spblock(std::move(upblock));
            ret->getTxBlocks().push_back(std::move(spblock));

            ++mTotalAdded;
            if (mTotalAdded >= mTotalFound) {
                // We are done..
                break;
            }
        }
    }

    return ret;
}

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org