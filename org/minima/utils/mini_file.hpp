#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <istream>

namespace org {
namespace minima {
namespace utils {
class Streamable;
namespace json {
class JSONObject;
}
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace utils {

class MiniFile {
public:
    // Create a base file path relative to GeneralParams::BASE_FILE_FOLDER when needed.
    // Ensures parent directories exist.
    static std::filesystem::path createBaseFile(const std::string& zFilename);

    // Write raw bytes to a file (overload without append)
    static void writeDataToFile(const std::filesystem::path& zFile,
                                const std::vector<std::uint8_t>& zData);

    // Write raw bytes to a file with optional append
    static void writeDataToFile(const std::filesystem::path& zFile,
                                const std::vector<std::uint8_t>& zData,
                                bool zAppend);

    // Write a Streamable object to a file (overload without append)
    static void writeObjectToFile(const std::filesystem::path& zFile,
                                  org::minima::utils::Streamable& zObject);

    // Write a Streamable object to a file with optional append
    static void writeObjectToFile(const std::filesystem::path& zFile,
                                  org::minima::utils::Streamable& zObject,
                                  bool zAppend);

    // Read a complete file into memory
    static std::vector<std::uint8_t> readCompleteFile(const std::filesystem::path& zFile);

    // Load a Streamable object from a file (fast path - reads whole file first)
    static void loadObject(const std::filesystem::path& zFile,
                           org::minima::utils::Streamable& zObject);

    // Load a Streamable object from a file using a buffered stream
    static void loadObjectSlow(const std::filesystem::path& zFile,
                               org::minima::utils::Streamable& zObject);

    // Load an encrypted Streamable (using PasswordCrypto) from a file
    static void loadObjectEncrypted(const std::string& zPassword,
                                    const std::filesystem::path& zFile,
                                    org::minima::utils::Streamable& zObject);

    // Save a Streamable object via MiniData caching
    static void saveObject(const std::filesystem::path& zFile,
                           org::minima::utils::Streamable& zObject);

    // Save a Streamable object directly to file with buffering
    static void saveObjectDirect(const std::filesystem::path& zFile,
                                 org::minima::utils::Streamable& zObject);

    // Save an encrypted Streamable object (using PasswordCrypto) to file
    static void saveObjectEncrypted(const std::string& zPassword,
                                    const std::filesystem::path& zFile,
                                    org::minima::utils::Streamable& zObject);

    // Copy a single file
    static void copyFile(const std::filesystem::path& zOrig,
                         const std::filesystem::path& zCopy);

    // Copy a file or a directory recursively
    static void copyFileOrFolder(const std::filesystem::path& zOrig,
                                 const std::filesystem::path& zCopy);

    // Delete a file or folder recursively, with parent path safety check
    static void deleteFileOrFolder(const std::string& mParentCheck,
                                   const std::filesystem::path& zFile);

    // Compute total size of a directory (recursive)
    static std::uintmax_t getTotalFileSize(const std::filesystem::path& zFolder);

    // Compute total size with child directory names up to max depth, place results in JSONObject
    static std::uintmax_t getTotalFileSizeWithNames(const std::filesystem::path& zFolder,
                                                    org::minima::utils::json::JSONObject& zResult,
                                                    int zMaxDepthInfo,
                                                    int zDepth);

    // Map filename to a content type (basic mapping)
    static std::string getContentType(const std::string& zFile);

    // Check if zChild path is a child (or equal) of zParent (lexically normalized)
    static bool isChild(const std::filesystem::path& zParent,
                        const std::filesystem::path& zChild);

    // Read all bytes from a std::istream
    static std::vector<std::uint8_t> readAllBytes(std::istream& inputStream);

    // GZIP decompress file
    static void decompressGzipFile(const std::filesystem::path& gzipFile,
                                   const std::filesystem::path& newFile);

    // GZIP compress file
    static void compressGzipFile(const std::filesystem::path& file,
                                 const std::filesystem::path& gzipFile);
};

} // namespace utils
} // namespace minima
} // namespace org