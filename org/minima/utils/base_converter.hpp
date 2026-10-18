#pragma once
#ifndef ORG_MINIMA_UTILS_BASE_CONVERTER_HPP
#define ORG_MINIMA_UTILS_BASE_CONVERTER_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {

class BaseConverter {
public:
    // BASE 16
    static std::string encode16(const std::vector<std::uint8_t>& bytes);
    static std::string encode16(const std::uint8_t* data, std::size_t len);
    static std::vector<std::uint8_t> decode16(const std::string& zHex);

    // Integer <-> Hex helpers
    static std::string numberToHex(int zNumber);
    static int hexToNumber(const std::string& zHex);

    // Custom BASE 32 with character substitutions
    static std::string encode32(const std::vector<std::uint8_t>& zData);
    static std::vector<std::uint8_t> decode32(const std::string& zBase32);
};

} // namespace utils
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_UTILS_BASE_CONVERTER_HPP