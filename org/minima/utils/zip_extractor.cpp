#include "org/minima/utils/zip_extractor.hpp"

#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <cstring>

#include <zip.h> // libzip

namespace org {
namespace minima {
namespace utils {

namespace {
    inline bool starts_with(const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
    }

    inline bool is_directory_entry(const std::string& name) {
        return !name.empty() && (name.back() == '/' || name.back() == '\\');
    }
}

std::filesystem::path ZipExtractor::newFile(const std::filesystem::path& destinationDir, const std::string& zipEntryName) {
    // Canonicalize the destination directory (must exist, similar to Java's getCanonicalPath)
    std::filesystem::path destDirCanonical;
    try {
        destDirCanonical = std::filesystem::canonical(destinationDir);
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(std::string("Failed to canonicalize destination directory: ") + e.what());
    }

    // Build a relative path from the entry name (strip any root so joining doesn't escape)
    std::filesystem::path entryPath(zipEntryName);
    entryPath = entryPath.relative_path();

    // Combine and normalize
    std::filesystem::path destFile = (destDirCanonical / entryPath).lexically_normal();

    // Logging if destination already exists (functional equivalent of MinimaLogger.log)
    if (std::filesystem::exists(destFile)) {
        std::cerr << "processing zip entry " << destinationDir.string() << " but it already exists" << std::endl;
    }

    // Prevent Zip Slip: ensure the destination is within the canonical destination directory
    const char sep = std::filesystem::path::preferred_separator;
    std::string destDirStr = destDirCanonical.u8string();
    if (!destDirStr.empty() && destDirStr.back() != sep) {
        destDirStr.push_back(sep);
    }
    const std::string destFileStr = destFile.u8string();

    if (!starts_with(destFileStr, destDirStr)) {
        throw std::runtime_error(std::string("Entry is outside of the target dir: ") + zipEntryName);
    }

    return destFile;
}

void ZipExtractor::unzip(std::istream& archive, const std::filesystem::path& destPath) {
    // Read entire input stream into memory (since libzip prefers random-access sources)
    std::vector<unsigned char> data;
    {
        constexpr std::size_t CHUNK = 4096;
        std::vector<char> buf(CHUNK);
        while (archive.good()) {
            archive.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            std::streamsize got = archive.gcount();
            if (got > 0) {
                data.insert(data.end(), buf.begin(), buf.begin() + got);
            }
            if (!archive) break;
        }
    }

    // Open zip from memory buffer using libzip
    zip_error_t ziperr;
    zip_error_init(&ziperr);
    zip_source_t* src = zip_source_buffer_create(
        data.data(),
        static_cast<zip_uint64_t>(data.size()),
        0, // do not let libzip free our buffer
        &ziperr
    );
    if (!src) {
        std::string msg = std::string("Failed to create zip source: ") + zip_error_strerror(&ziperr);
        zip_error_fini(&ziperr);
        throw std::runtime_error(msg);
    }

    zip_t* za = zip_open_from_source(src, 0, &ziperr);
    if (!za) {
        std::string msg = std::string("Failed to open zip from source: ") + zip_error_strerror(&ziperr);
        zip_source_free(src); // opening failed, we own src
        zip_error_fini(&ziperr);
        throw std::runtime_error(msg);
    }
    zip_error_fini(&ziperr);

    try {
        zip_int64_t num_entries = zip_get_num_entries(za, 0);
        if (num_entries < 0) {
            zip_error_t* err = zip_get_error(za);
            std::string msg = std::string("Failed to get zip entries: ") + (err ? zip_error_strerror(err) : "unknown error");
            throw std::runtime_error(msg);
        }

        // Buffer similar to Java's 1024 bytes
        std::vector<char> buffer(1024);

        for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(num_entries); ++i) {
            zip_stat_t st;
            if (zip_stat_index(za, i, ZIP_FL_ENC_GUESS, &st) != 0) {
                zip_error_t* err = zip_get_error(za);
                std::string msg = std::string("Failed to stat zip entry at index ")
                                  + std::to_string(i) + ": " + (err ? zip_error_strerror(err) : "unknown error");
                throw std::runtime_error(msg);
            }

            const std::string entryName = st.name ? std::string(st.name) : std::string();
            const bool isDir = is_directory_entry(entryName);

            // Compute safe destination file path
            std::filesystem::path outPath = newFile(destPath, entryName);

            if (isDir) {
                // Create directory if needed
                if (!std::filesystem::is_directory(outPath)) {
                    std::error_code ec;
                    if (!std::filesystem::create_directories(outPath, ec) && ec) {
                        throw std::runtime_error(std::string("Failed to create directory ") + outPath.string());
                    }
                }
            } else {
                // Ensure parent directories exist
                std::filesystem::path parent = outPath.parent_path();
                if (!parent.empty() && !std::filesystem::is_directory(parent)) {
                    std::error_code ec;
                    if (!std::filesystem::create_directories(parent, ec) && ec) {
                        throw std::runtime_error(std::string("Failed to create directory ") + parent.string());
                    }
                }

                // Open the entry for reading
                zip_file_t* zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    zip_error_t* err = zip_get_error(za);
                    std::string msg = std::string("Failed to open zip entry: ") + entryName + " - "
                                      + (err ? zip_error_strerror(err) : "unknown error");
                    throw std::runtime_error(msg);
                }

                // Write file content
                std::ofstream ofs(outPath, std::ios::binary);
                if (!ofs) {
                    zip_fclose(zf);
                    throw std::runtime_error(std::string("Failed to open file for writing: ") + outPath.string());
                }

                while (true) {
                    zip_int64_t readlen = zip_fread(zf, buffer.data(), buffer.size());
                    if (readlen < 0) {
                        zip_error_t* ferr = zip_file_get_error(zf);
                        std::string msg = std::string("Error reading zip entry: ") + entryName + " - "
                                          + (ferr ? zip_error_strerror(ferr) : "unknown error");
                        zip_fclose(zf);
                        throw std::runtime_error(msg);
                    }
                    if (readlen == 0) {
                        break;
                    }
                    ofs.write(buffer.data(), static_cast<std::streamsize>(readlen));
                    if (!ofs) {
                        zip_fclose(zf);
                        throw std::runtime_error(std::string("Failed to write to file: ") + outPath.string());
                    }
                }

                zip_fclose(zf);
                ofs.close();
            }
        }

        // Close ZIP
        if (zip_close(za) != 0) {
            zip_error_t* err = zip_get_error(za);
            std::string msg = std::string("Failed to close zip archive: ")
                              + (err ? zip_error_strerror(err) : "unknown error");
            throw std::runtime_error(msg);
        }
    } catch (...) {
        // Ensure the archive is discarded to free resources on error
        zip_discard(za);
        throw;
    }
}

} // namespace utils
} // namespace minima
} // namespace org