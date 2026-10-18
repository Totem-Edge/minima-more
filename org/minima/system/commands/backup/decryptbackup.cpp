#include "org/minima/system/commands/backup/decryptbackup.hpp"

#include <sstream>
#include <fstream>
#include <stdexcept>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_list.hpp"
#include "org/minima/utils/encrypt/generate_key.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::utils::json::JSONObject;
using org::minima::utils::MiniFile;
using org::minima::utils::MiniFormat;
using org::minima::objects::base::MiniData;
using org::minima::txpowdb::sql::TxPoWList;
using org::minima::utils::encrypt::GenerateKey;
using org::minima::utils::encrypt::Cipher;

decryptbackup::decryptbackup()
    : org::minima::system::commands::Command(
          "decryptbackup",
          "[file:] (password:) (output:)- Decrypt an encrypted backup.") {}

std::string decryptbackup::getFullHelp() const {
    return std::string()
           + "\ndecryptbackup \n"
           + "\n"
           + "Decrypt an encrypted backup.\n"
           + "\n"
           + "file:\n"
           + "    Specify the filename or local path of the backup to restore\n"
           + "\n"
           + "password: (optional)\n"
           + "    Enter the password of the backup \n"
           + "\n"
           + "output: (optional)\n"
           + "    Specify the output file \n"
           + "\n"
           + "Examples:\n"
           + "\n"
           + "decryptbackup file:my-full-backup-01-Jan-22 password:Longsecurepassword456\n";
}

std::vector<std::string> decryptbackup::getValidParams() const {
    return { "file", "password", "output" };
}

std::unique_ptr<JSONObject> decryptbackup::runCommand() {
    auto ret = getJSONReply();

    // Params
    const std::string file = getParam("file");
    const std::string password = getParam("password", "minima");
    if (password == "") {
        throw org::minima::system::commands::CommandException("Cannot have a blank password");
    }

    // Input file existence
    const std::filesystem::path restorefile = MiniFile::createBaseFile(file);
    if (!std::filesystem::exists(restorefile)) {
        throw std::runtime_error("Restore file doesn't exist : " + std::filesystem::absolute(restorefile).string());
    }

    // Output file
    std::string outfile = getParam("output", "");
    if (outfile.empty()) {
        outfile = std::string("decrypted-") + restorefile.filename().string();
    }
    const std::filesystem::path outputfile = MiniFile::createBaseFile(outfile);
    if (std::filesystem::exists(outputfile)) {
        std::error_code ec;
        std::filesystem::remove(outputfile, ec);
    }

    // Read the whole file
    const std::vector<std::uint8_t> restoredata = MiniFile::readCompleteFile(restorefile);

    // Construct an in-memory input stream
    const std::string rdata(reinterpret_cast<const char*>(restoredata.data()), restoredata.size());
    std::istringstream in(rdata, std::ios::binary);

    // Read SALT and IV MiniData
    MiniData salt = MiniData::ReadFromStream(in);
    MiniData ivparam = MiniData::ReadFromStream(in);

    // Derive AES key from password and salt
    auto sk = GenerateKey::secretKey(password, salt.getBytes());

    // Remaining bytes are AES-CBC-PKCS5 encrypted GZIP stream
    std::vector<std::uint8_t> encdata = MiniFile::readAllBytes(in);

    // Decrypt
    Cipher ciph = GenerateKey::getCipherSYM(Cipher::DECRYPT_MODE, ivparam.getBytes(), sk.bytes());
    std::vector<std::uint8_t> decryptedGzip = ciph.doFinal(encdata);

    // Write decrypted GZIP to a temporary file, then decompress to the output
    std::filesystem::path outdir = outputfile.has_parent_path() ? outputfile.parent_path()
                                                                : std::filesystem::current_path();
    const std::string tmpname = std::string("tmp-decrypt-") + MiniFormat::createRandomString(12) + ".gz";
    const std::filesystem::path tmpgz = outdir / tmpname;

    MiniFile::writeDataToFile(tmpgz, decryptedGzip);

    try {
        MiniFile::decompressGzipFile(tmpgz, outputfile);
    } catch (...) {
        // Clean up temp file then rethrow as incorrect password
        std::error_code ec;
        std::filesystem::remove(tmpgz, ec);
        throw org::minima::system::commands::CommandException("Incorrect Password!");
    }

    // Remove temp file
    {
        std::error_code ec;
        std::filesystem::remove(tmpgz, ec);
    }

    // Build response
    JSONObject resp;
    resp.put("input", std::filesystem::absolute(restorefile).string());
    resp.put("inputsize", MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(restorefile))));
    resp.put("output", std::filesystem::absolute(outputfile).string());
    resp.put("outputsize", MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(outputfile))));
    resp.put("message", std::string("You can now open the output file in a HEX editor to get the seed if your backup was not locked"));

    ret->put("response", resp);
    return ret;
}

long long decryptbackup::readNextBackup(const std::filesystem::path& zOutput, std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    MiniFile::writeDataToFile(zOutput, data.getBytes());
    return static_cast<long long>(std::filesystem::file_size(zOutput));
}

std::unique_ptr<TxPoWList> decryptbackup::readNextTxPoWList(std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    return TxPoWList::convertMiniDataVersion(data);
}

org::minima::system::commands::Command* decryptbackup::getFunction() {
    return new decryptbackup();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org