#pragma once

#include <istream>
#include <filesystem>
#include <string>

namespace org {
namespace minima {
namespace utils {

class ZipExtractor {
public:
    // Functional equivalent of: public static void unzip(InputStream archive, File zDestPath) throws IOException
    // Throws std::runtime_error on failure.
    static void unzip(std::istream& archive, const std::filesystem::path& destPath);

private:
    // Functional equivalent of the Java private static File newFile(File destinationDir, ZipEntry zipEntry)
    // Validates the entry path is within destinationDir and returns the normalized destination path.
    static std::filesystem::path newFile(const std::filesystem::path& destinationDir, const std::string& zipEntryName);
};

} // namespace utils
} // namespace minima
} // namespace org