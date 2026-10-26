#pragma once

// UTF-8 <-> UTF-16 helpers used to reproduce Java String semantics.
//
// Minima's Java reference stores scripts/strings as java.lang.String and
// MiniString decodes bytes with StandardCharsets.UTF_8. String.length() and
// String.substring() therefore operate on UTF-16 code units, not UTF-8 bytes.
// KISSVM LEN/SUBSTR must match that for non-ASCII scripts.

#include <cstdint>
#include <string>

namespace org {
namespace minima {
namespace utils {
namespace utf16 {

// Decode UTF-8 bytes into UTF-16 code units, mirroring
// new String(bytes, StandardCharsets.UTF_8). Malformed input is replaced with
// U+FFFD. The malformed-sequence consumption policy here advances one byte at a
// time; valid UTF-8 (the realistic case) round-trips exactly.
inline std::u16string from_utf8(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());

    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const std::size_t n = s.size();
    std::size_t i = 0;

    while (i < n) {
        const unsigned char b = p[i];

        if (b < 0x80) {
            out.push_back(static_cast<char16_t>(b));
            ++i;
            continue;
        }

        if (b >= 0xC2 && b <= 0xDF) {
            if (i + 1 < n && (p[i + 1] & 0xC0) == 0x80) {
                const char16_t cp = static_cast<char16_t>(((b & 0x1Fu) << 6) | (p[i + 1] & 0x3Fu));
                out.push_back(cp);
                i += 2;
                continue;
            }
        } else if (b >= 0xE0 && b <= 0xEF) {
            if (i + 2 < n && (p[i + 1] & 0xC0) == 0x80 && (p[i + 2] & 0xC0) == 0x80) {
                const char16_t cp = static_cast<char16_t>(((b & 0x0Fu) << 12) |
                                                          ((p[i + 1] & 0x3Fu) << 6) |
                                                          (p[i + 2] & 0x3Fu));
                // Reject overlong encodings and UTF-16 surrogate code points.
                if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF)) {
                    out.push_back(cp);
                    i += 3;
                    continue;
                }
            }
        } else if (b >= 0xF0 && b <= 0xF4) {
            if (i + 3 < n && (p[i + 1] & 0xC0) == 0x80 && (p[i + 2] & 0xC0) == 0x80 &&
                (p[i + 3] & 0xC0) == 0x80) {
                std::uint32_t cp = (static_cast<std::uint32_t>(b & 0x07u) << 18) |
                                   (static_cast<std::uint32_t>(p[i + 1] & 0x3Fu) << 12) |
                                   (static_cast<std::uint32_t>(p[i + 2] & 0x3Fu) << 6) |
                                   static_cast<std::uint32_t>(p[i + 3] & 0x3Fu);
                if (cp >= 0x10000u && cp <= 0x10FFFFu) {
                    cp -= 0x10000u;
                    out.push_back(static_cast<char16_t>(0xD800u + (cp >> 10)));
                    out.push_back(static_cast<char16_t>(0xDC00u + (cp & 0x3FFu)));
                    i += 4;
                    continue;
                }
            }
        }

        out.push_back(static_cast<char16_t>(0xFFFD));
        ++i;
    }

    return out;
}

// Encode UTF-16 code units as UTF-8, mirroring
// String.getBytes(StandardCharsets.UTF_8). Unpaired surrogates are replaced
// with '?' as Java's UTF-8 encoder does.
inline std::string to_utf8(const std::u16string& u) {
    std::string out;
    out.reserve(u.size());

    for (std::size_t i = 0; i < u.size(); ++i) {
        const std::uint32_t c = static_cast<std::uint16_t>(u[i]);

        if (c < 0x80u) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (c >> 6)));
            out.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        } else if (c >= 0xD800u && c <= 0xDBFFu) {
            if (i + 1 < u.size()) {
                const std::uint32_t lo = static_cast<std::uint16_t>(u[i + 1]);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    const std::uint32_t cp = 0x10000u + ((c - 0xD800u) << 10) + (lo - 0xDC00u);
                    out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
                    out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
                    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
                    ++i;
                    continue;
                }
            }
            out.push_back('?');
        } else if (c >= 0xDC00u && c <= 0xDFFFu) {
            out.push_back('?');
        } else {
            out.push_back(static_cast<char>(0xE0u | (c >> 12)));
            out.push_back(static_cast<char>(0x80u | ((c >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        }
    }

    return out;
}

} // namespace utf16
} // namespace utils
} // namespace minima
} // namespace org
