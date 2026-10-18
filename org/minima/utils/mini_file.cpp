#include "org/minima/utils/mini_file.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/encrypt/password_crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/streamable.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <system_error>
#include <cstdio>
#include <cstring>

#include <zlib.h>

namespace org {
namespace minima {
namespace utils {

namespace fs = std::filesystem;

static void logError(const std::string& msg) {
    std::cerr << msg << std::endl;
}

static std::string formatSize(std::uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream oss;
    if (unit == 0) {
        oss << static_cast<std::uint64_t>(value) << " " << units[unit];
    } else {
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << value << " " << units[unit];
    }
    return oss.str();
}

static void ensureParentExists(const fs::path& p) {
    std::error_code ec;
    auto parent = p.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }
}

fs::path MiniFile::createBaseFile(const std::string& zFilename) {
    fs::path retfile;

    // Does name contain a slash or backslash.. treat as absolute-like path
    if (zFilename.find(fs::path::preferred_separator) != std::string::npos ||
        zFilename.find('\\') != std::string::npos ||
        zFilename.find('/') != std::string::npos) {
        retfile = fs::path(zFilename);
    } else if (org::minima::system::params::GeneralParams::BASE_FILE_FOLDER.empty()) {
        retfile = fs::path(zFilename);
    } else {
        retfile = fs::path(org::minima::system::params::GeneralParams::BASE_FILE_FOLDER) / zFilename;
    }

    // Ensure parent exists
    ensureParentExists(retfile);
    return retfile;
}

void MiniFile::writeDataToFile(const fs::path& zFile, const std::vector<std::uint8_t>& zData) {
    MiniFile::writeDataToFile(zFile, zData, false);
}

void MiniFile::writeDataToFile(const fs::path& zFile,
                               const std::vector<std::uint8_t>& zData,
                               bool zAppend) {
    // Check Parent
    ensureParentExists(zFile);

    std::error_code ec;
    bool exists = fs::exists(zFile, ec);

    if (exists) {
        if (!zAppend) {
            // Delete and recreate
            fs::remove(zFile, ec);
            // Create new empty file
            std::ofstream ofs(zFile, std::ios::binary | std::ios::out | std::ios::trunc);
            ofs.close();
        }
    } else {
        // Create new file
        std::ofstream ofs(zFile, std::ios::binary | std::ios::out | std::ios::trunc);
        ofs.close();
    }

    // Write it out..
    std::ofstream fos(zFile, std::ios::binary | std::ios::out | (zAppend ? std::ios::app : std::ios::out));
    if (!fos) {
        throw std::ios_base::failure("Failed to open file for writing: " + zFile.string());
    }

    if (!zData.empty()) {
        fos.write(reinterpret_cast<const char*>(zData.data()), static_cast<std::streamsize>(zData.size()));
    }

    // flush
    fos.flush();
    fos.close();
}

void MiniFile::writeObjectToFile(const fs::path& zFile, org::minima::utils::Streamable& zObject) {
    MiniFile::writeObjectToFile(zFile, zObject, false);
}

void MiniFile::writeObjectToFile(const fs::path& zFile,
                                 org::minima::utils::Streamable& zObject,
                                 bool zAppend) {
    // First write the object to a memory structure
    std::ostringstream baos(std::ios::binary);
    zObject.writeDataStream(baos);

    baos.flush();
    const std::string data = baos.str();
    std::vector<std::uint8_t> bytes(data.begin(), data.end());

    // Check Parent and write
    MiniFile::writeDataToFile(zFile, bytes, zAppend);
}

std::vector<std::uint8_t> MiniFile::readCompleteFile(const fs::path& zFile) {
    std::ifstream fis(zFile, std::ios::binary);
    if (!fis) {
        throw std::ios_base::failure("Failed to open file for reading: " + zFile.string());
    }

    fis.seekg(0, std::ios::end);
    std::streamsize size = fis.tellg();
    if (size < 0) {
        size = 0;
    }
    fis.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> ret;
    ret.resize(static_cast<std::size_t>(size));

    if (size > 0) {
        fis.read(reinterpret_cast<char*>(ret.data()), size);
        // In case of short read, resize to actual read
        std::streamsize got = fis.gcount();
        if (got < size) {
            ret.resize(static_cast<std::size_t>(got));
        }
    }

    fis.close();
    return ret;
}

void MiniFile::loadObject(const fs::path& zFile, org::minima::utils::Streamable& zObject) {
    std::error_code ec;
    if (!fs::exists(zFile, ec)) {
        logError("Load Object file does not exist : " + zFile.string());
        return;
    }

    try {
        // Read whole file fast
        std::vector<std::uint8_t> data = MiniFile::readCompleteFile(zFile);

        // Convert to a Streamable object
        std::string sdata(data.begin(), data.end());
        std::istringstream bais(sdata, std::ios::binary);
        zObject.readDataStream(bais);
    } catch (const std::exception& e) {
        logError(std::string("Exception in loadObject: ") + e.what());
    }
}

void MiniFile::loadObjectSlow(const fs::path& zFile, org::minima::utils::Streamable& zObject) {
    std::error_code ec;
    if (!fs::exists(zFile, ec)) {
        logError("Load Object file does not exist: " + zFile.string());
        return;
    }
    
    try {
        std::ifstream fis(zFile, std::ios::binary);
        if (!fis) {
            throw std::ios_base::failure("Failed to open file for reading (slow): " + zFile.string());
        }
        
        // Buffered by default, directly pass stream
        zObject.readDataStream(fis);
        fis.close();
        
    } catch (const std::exception& e) {
        logError(std::string("Exception in loadObjectSlow: ") + e.what());
    }
}


void MiniFile::loadObjectEncrypted(const std::string& zPassword,
                                   const fs::path& zFile,
                                   org::minima::utils::Streamable& zObject) {
    std::error_code ec;
    if (!fs::exists(zFile, ec)) {
        logError("Load Object file does not exist : " + zFile.string());
        return;
    }

    try {
        // Read whole file
        std::vector<std::uint8_t> data = MiniFile::readCompleteFile(zFile);

        // Now decrypt
        org::minima::objects::base::MiniData decrypted =
            org::minima::utils::encrypt::PasswordCrypto::decryptPassword(
                zPassword, org::minima::objects::base::MiniData(data));

        // Convert to Streamable
        const auto& dbytes = decrypted.getBytes();
        std::string sdata(dbytes.begin(), dbytes.end());
        std::istringstream bais(sdata, std::ios::binary);
        zObject.readDataStream(bais);
    } catch (const std::exception& e) {
        logError(std::string("Exception in loadObjectEncrypted: ") + e.what());
    }
}

void MiniFile::saveObject(const fs::path& zFile, org::minima::utils::Streamable& zObject) {
    try {
        // Write into byte array via MiniData
        std::unique_ptr<org::minima::objects::base::MiniData> casc =
            org::minima::objects::base::MiniData::getMiniDataVersion(zObject);

        if (!casc) {
            logError("saveObject: getMiniDataVersion returned null");
            return;
        }

        // save to disk
        MiniFile::writeDataToFile(zFile, casc->getBytes());
    } catch (const std::exception& exc) {
        logError(std::string("Exception in saveObject: ") + exc.what());
    }
}

void MiniFile::saveObjectDirect(const fs::path& zFile, org::minima::utils::Streamable& zObject) {
    // Check Parent
    ensureParentExists(zFile);

    try {
        std::error_code ec;

        // Delete the old..
        if (fs::exists(zFile, ec)) {
            fs::remove(zFile, ec);
        }

        // Write it out..
        std::ofstream fos(zFile, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!fos) {
            throw std::ios_base::failure("Failed to open file for writing: " + zFile.string());
        }

        // Write data
        zObject.writeDataStream(fos);

        // flush
        fos.flush();
        fos.close();
    } catch (const std::exception& exc) {
        logError(std::string("Exception in saveObjectDirect: ") + exc.what());
    }
}

void MiniFile::saveObjectEncrypted(const std::string& zPassword,
                                   const fs::path& zFile,
                                   org::minima::utils::Streamable& zObject) {
    try {
        // Write into byte array
        std::unique_ptr<org::minima::objects::base::MiniData> casc =
            org::minima::objects::base::MiniData::getMiniDataVersion(zObject);

        if (!casc) {
            logError("saveObjectEncrypted: getMiniDataVersion returned null");
            return;
        }

        // Convert to an encrypted object
        org::minima::objects::base::MiniData encrypted =
            org::minima::utils::encrypt::PasswordCrypto::encryptPassword(zPassword, *casc);

        // save to disk
        MiniFile::writeDataToFile(zFile, encrypted.getBytes());
    } catch (const std::exception& exc) {
        logError(std::string("Exception in saveObjectEncrypted: ") + exc.what());
    }
}

void MiniFile::copyFile(const fs::path& zOrig, const fs::path& zCopy) {
    std::error_code ec;
    if (!fs::exists(zOrig, ec)) {
        logError("Trying to copy file that does not exist " + zOrig.string());
        return;
    }

    std::ifstream is(zOrig, std::ios::binary);
    std::ofstream os(zCopy, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!is || !os) {
        throw std::ios_base::failure("Failed to open files for copy: " + zOrig.string() + " -> " + zCopy.string());
    }

    std::vector<char> buffer(16384);
    while (is) {
        is.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize readLen = is.gcount();
        if (readLen > 0) {
            os.write(buffer.data(), readLen);
        }
    }
    is.close();
    os.close();
}

void MiniFile::copyFileOrFolder(const fs::path& zOrig, const fs::path& zCopy) {
    std::error_code ec;
    if (!fs::exists(zOrig, ec)) {
        logError("Trying to copy file that does not exist " + zOrig.string());
        return;
    }

    if (fs::is_directory(zOrig, ec)) {
        // Make the new dir
        fs::create_directories(zCopy, ec);

        // Now scan through and recurse..
        for (const auto& entry : fs::directory_iterator(zOrig, ec)) {
            const fs::path& child = entry.path();
            fs::path newfile = zCopy / child.filename();
            copyFileOrFolder(child, newfile);
        }
    } else {
        // Just copy the file
        copyFile(zOrig, zCopy);
    }
}

void MiniFile::deleteFileOrFolder(const std::string& mParentCheck, const fs::path& zFile) {
    std::error_code ec;
    if (zFile.empty() || !fs::exists(zFile, ec)) {
        return;
    }

    // If Directory, recurse
    if (fs::is_directory(zFile, ec)) {
        for (const auto& entry : fs::directory_iterator(zFile, ec)) {
            deleteFileOrFolder(mParentCheck, entry.path());
        }
    }

    // Now delete the actual file (double check is child of parent check)
    bool delok = false;
    fs::path abspath = fs::absolute(zFile, ec);
    std::string abs = abspath.string();

    if (mParentCheck.empty()) {
        delok = fs::remove(zFile, ec);
    } else {
        // start-with check
        if (abs.rfind(mParentCheck, 0) == 0) {
            delok = fs::remove(zFile, ec);
        } else {
            logError("Attempt to delete File NOT child of parent check " + abs + " / " + mParentCheck);
        }
    }

    if (!delok) {
        logError("ERROR deleting file " + abs);
    }
}

std::uintmax_t MiniFile::getTotalFileSize(const fs::path& zFolder) {
    std::error_code ec;
    std::uintmax_t tot = 0;

    if (!fs::exists(zFolder, ec)) {
        return 0;
    }

    for (const auto& entry : fs::directory_iterator(zFolder, ec)) {
        const fs::path& p = entry.path();
        if (fs::is_directory(p, ec)) {
            tot += getTotalFileSize(p);
        } else {
            std::uintmax_t sz = 0;
            std::error_code ec2;
            sz = fs::file_size(p, ec2);
            if (!ec2) {
                tot += sz;
            }
        }
    }
    return tot;
}

std::uintmax_t MiniFile::getTotalFileSizeWithNames(const fs::path& zFolder,
                                                   org::minima::utils::json::JSONObject& zResult,
                                                   int zMaxDepthInfo,
                                                   int zDepth) {
    org::minima::utils::json::JSONObject dirs;

    std::uintmax_t tot = 0;

    std::error_code ec;
    if (!fs::exists(zFolder, ec)) {
        return 0;
    }

    for (const auto& entry : fs::directory_iterator(zFolder, ec)) {
        const fs::path& p = entry.path();
        if (fs::is_directory(p, ec)) {
            org::minima::utils::json::JSONObject dirdata;
            std::uintmax_t dirsize = getTotalFileSizeWithNames(p, dirdata, zMaxDepthInfo, zDepth + 1);
            tot += dirsize;

            dirs.put(p.filename().string(), dirdata);
        } else {
            std::error_code ec2;
            std::uintmax_t sz = fs::file_size(p, ec2);
            if (!ec2) {
                tot += sz;
            }
        }
    }

    zResult.put("total", formatSize(static_cast<std::uint64_t>(tot)));

    if (zDepth < zMaxDepthInfo) {
        if (dirs.size() > 0) {
            zResult.put("dirs", dirs);
        }
    }

    return tot;
}

std::string MiniFile::getContentType(const std::string& zFile) {
    std::string ending;
    std::size_t dot = zFile.find_last_of('.');
    if (dot != std::string::npos) {
        ending = zFile.substr(dot + 1);
    } else {
        return "text/plain";
    }

    if (ending == "html") {
        return "text/html";
    } else if (ending == "htm") {
        return "text/html";
    } else if (ending == "css") {
        return "text/css";
    } else if (ending == "js") {
        return "text/javascript";
    } else if (ending == "txt") {
        return "text/plain";
    } else if (ending == "xml") {
        return "text/xml";
    } else if (ending == "jpg") {
        return "image/jpeg";
    } else if (ending == "jpeg") {
        return "image/jpeg";
    } else if (ending == "png") {
        return "image/png";
    } else if (ending == "gif") {
        return "image/gif";
    } else if (ending == "svg") {
        return "image/svg+xml";
    } else if (ending == "ico") {
        return "image/ico";
    } else if (ending == "ttf") {
        return "font/ttf";
    } else if (ending == "zip") {
        return "application/zip";
    } else if (ending == "pdf") {
        return "application/pdf";
    } else if (ending == "wasm") {
        return "application/wasm";
    } else if (ending == "mp3") {
        return "audio/mp3";
    } else if (ending == "wav") {
        return "audio/wav";
    }

    return "text/plain";
}

bool MiniFile::isChild(const fs::path& zParent, const fs::path& zChild) {
    fs::path pparent = fs::weakly_canonical(zParent);
    fs::path pfile   = fs::weakly_canonical(zChild);

    std::string parentAbs = pparent.string();
    std::string fileAbs   = pfile.string();

#ifdef _WIN32
    // Windows paths are case-insensitive
    std::transform(parentAbs.begin(), parentAbs.end(), parentAbs.begin(), ::tolower);
    std::transform(fileAbs.begin(), fileAbs.end(), fileAbs.begin(), ::tolower);
#endif

    return fileAbs.rfind(parentAbs, 0) == 0;
}

std::vector<std::uint8_t> MiniFile::readAllBytes(std::istream& inputStream) {
    const std::size_t bufLen = 1024;
    std::vector<std::uint8_t> out;
    out.reserve(bufLen * 4);

    std::vector<char> buf(bufLen);
    while (true) {
        inputStream.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize readLen = inputStream.gcount();
        if (readLen > 0) {
            out.insert(out.end(), reinterpret_cast<std::uint8_t*>(buf.data()),
                       reinterpret_cast<std::uint8_t*>(buf.data()) + readLen);
        }
        if (readLen < static_cast<std::streamsize>(buf.size())) {
            break; // EOF or short read
        }
    }
    return out;
}

void MiniFile::decompressGzipFile(const fs::path& gzipFile, const fs::path& newFile) {
    try {
        gzFile gz = gzopen(gzipFile.string().c_str(), "rb");
        if (!gz) {
            logError("Failed to open gzip file for reading: " + gzipFile.string());
            return;
        }

        std::ofstream fos(newFile, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!fos) {
            logError("Failed to open output file for writing: " + newFile.string());
            gzclose(gz);
            return;
        }

        std::vector<char> buffer(1024);
        int len = 0;
        while ((len = gzread(gz, buffer.data(), static_cast<unsigned int>(buffer.size()))) > 0) {
            fos.write(buffer.data(), len);
        }

        fos.close();
        gzclose(gz);
    } catch (const std::exception& e) {
        std::cerr << "decompressGzipFile exception: " << e.what() << std::endl;
    }
}

void MiniFile::compressGzipFile(const fs::path& file, const fs::path& gzipFile) {
    try {
        std::ifstream fis(file, std::ios::binary);
        if (!fis) {
            std::cerr << "compressGzipFile: cannot open input file " << file << std::endl;
            return;
        }

        gzFile gz = gzopen(gzipFile.string().c_str(), "wb");
        if (!gz) {
            std::cerr << "compressGzipFile: cannot open output gzip file " << gzipFile << std::endl;
            return;
        }

        std::vector<char> buffer(1024);
        while (fis) {
            fis.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize len = fis.gcount();
            if (len > 0) {
                int written = gzwrite(gz, buffer.data(), static_cast<unsigned int>(len));
                if (written == 0) {
                    // gzwrite error
                    int errnum = 0;
                    const char* err = gzerror(gz, &errnum);
                    std::cerr << "compressGzipFile gzwrite error: " << (err ? err : "unknown") << std::endl;
                    break;
                }
            }
        }

        gzclose(gz);
        fis.close();
    } catch (const std::exception& e) {
        std::cerr << "compressGzipFile exception: " << e.what() << std::endl;
    }
}

} // namespace utils
} // namespace minima
} // namespace org