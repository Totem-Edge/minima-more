#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.hpp"

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <vector>

#include <openssl/evp.h>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

MegaMMRBackup::MegaMMRBackup()
    : mMegaMMR(nullptr), mIBD(nullptr) {}

MegaMMRBackup::MegaMMRBackup(
    org::minima::objects::mmr::MegaMMR* zMMR,
    org::minima::objects::IBD* zIDB)
    : mMegaMMR(zMMR), mIBD(zIDB) {}

MegaMMRBackup::~MegaMMRBackup() = default;

void MegaMMRBackup::setMegaMMR(org::minima::objects::mmr::MegaMMR* zMMR) {
    mOwnedMMR.reset();
    mMegaMMR = zMMR;
}

void MegaMMRBackup::setIBD(org::minima::objects::IBD* zIDB) {
    mOwnedIBD.reset();
    mIBD = zIDB;
}

org::minima::objects::mmr::MegaMMR* MegaMMRBackup::getMegaMMR() {
    return mMegaMMR;
}

org::minima::objects::IBD* MegaMMRBackup::getIBD() {
    return mIBD;
}

std::unique_ptr<org::minima::objects::IBD> MegaMMRBackup::takeIBD() {
    auto out = std::move(mOwnedIBD);
    mIBD = nullptr;
    return out;
}

std::unique_ptr<org::minima::objects::mmr::MegaMMR> MegaMMRBackup::takeMegaMMR() {
    auto out = std::move(mOwnedMMR);
    mMegaMMR = nullptr;
    return out;
}

std::string MegaMMRBackup::fileSha256(const std::filesystem::path& zFile) {
    std::ifstream in(zFile, std::ios::binary);
    if (!in) {
        throw std::runtime_error("restore manifest: cannot open " + zFile.string());
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("restore manifest: EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("restore manifest: SHA-256 init failed");
    }

    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize n = in.gcount();
        if (n > 0) EVP_DigestUpdate(ctx, buf.data(), static_cast<std::size_t>(n));
    }

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdlen = 0;
    EVP_DigestFinal_ex(ctx, md, &mdlen);
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(mdlen * 2);
    for (unsigned int i = 0; i < mdlen; ++i) {
        out.push_back(hex[md[i] >> 4]);
        out.push_back(hex[md[i] & 0x0f]);
    }
    return out;
}

RestoreManifest MegaMMRBackup::buildManifest(MegaMMRBackup& zBackup,
                                             const std::filesystem::path& zPayload) {
    RestoreManifest m;
    m.present = true;
    m.version = 2;
    m.network = "minima";

    auto* mega = zBackup.getMegaMMR();
    if (mega) {
        m.tip_block = mega->getMMR().getBlockTime().getAsLong();
        m.first_block = mega->firstBlock();
        m.coins = static_cast<long long>(mega->coinCount());
        m.mmr_entries = mega->getMMR().getTotalEntries();
    }

    auto* ibd = zBackup.getIBD();
    if (ibd) {
        // An empty IBD (legacy/partial backup) has no tree tip; getTreeTip()
        // indexes the block vector.
        m.tree_tip = ibd->getTxBlocks().empty()
                         ? -1
                         : ibd->getTreeTip().getAsLong();
        m.ibd_blocks = static_cast<long long>(ibd->getTxBlocks().size());
        if (auto* casc = ibd->getCascade()) {
            if (auto tip = casc->getTip()) {
                m.cascade_tip = tip->getTxPoW().getBlockNumber().getAsLong();
                m.tip_hash = tip->getTxPoW().getTxPoWID();
            }
            m.weight = casc->getTotalWeight().toString();
        }
        if (!ibd->getTxBlocks().empty()) {
            m.tree_root =
                ibd->getTxBlocks().front()->getTxPoW().getBlockNumber().getAsLong();
            if (m.tip_hash.empty()) {
                m.tip_hash =
                    ibd->getTxBlocks().back()->getTxPoW().getTxPoWID();
            }
        }
    }

    m.sha256 = fileSha256(zPayload);
    const std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    m.created_at = ts;
    return m;
}

bool MegaMMRBackup::writeManifest(const std::filesystem::path& zFile,
                                  const RestoreManifest& zManifest) {
    std::ofstream out(zFile, std::ios::out | std::ios::trunc);
    if (!out) return false;
    out << "format=hawk-megammr-backup\n";
    out << "version=" << zManifest.version << "\n";
    out << "network=" << zManifest.network << "\n";
    out << "tip_block=" << zManifest.tip_block << "\n";
    out << "tip_hash=" << zManifest.tip_hash << "\n";
    out << "weight=" << zManifest.weight << "\n";
    out << "first_block=" << zManifest.first_block << "\n";
    out << "coins=" << zManifest.coins << "\n";
    out << "mmr_entries=" << zManifest.mmr_entries << "\n";
    out << "cascade_tip=" << zManifest.cascade_tip << "\n";
    out << "tree_root=" << zManifest.tree_root << "\n";
    out << "tree_tip=" << zManifest.tree_tip << "\n";
    out << "ibd_blocks=" << zManifest.ibd_blocks << "\n";
    out << "sha256=" << zManifest.sha256 << "\n";
    out << "created_at=" << zManifest.created_at << "\n";
    out.flush();
    return static_cast<bool>(out);
}

RestoreManifest MegaMMRBackup::readManifest(const std::filesystem::path& zFile) {
    RestoreManifest m;
    std::ifstream in(zFile);
    if (!in) return m;

    std::string line;
    std::string format;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        try {
            if (key == "format") format = val;
            else if (key == "version") m.version = std::stoi(val);
            else if (key == "network") m.network = val;
            else if (key == "tip_block") m.tip_block = std::stoll(val);
            else if (key == "tip_hash") m.tip_hash = val;
            else if (key == "weight") m.weight = val;
            else if (key == "first_block") m.first_block = std::stoll(val);
            else if (key == "coins") m.coins = std::stoll(val);
            else if (key == "mmr_entries") m.mmr_entries = std::stoll(val);
            else if (key == "cascade_tip") m.cascade_tip = std::stoll(val);
            else if (key == "tree_root") m.tree_root = std::stoll(val);
            else if (key == "tree_tip") m.tree_tip = std::stoll(val);
            else if (key == "ibd_blocks") m.ibd_blocks = std::stoll(val);
            else if (key == "sha256") m.sha256 = val;
            else if (key == "created_at") m.created_at = val;
        } catch (...) {
            // A malformed numeric field leaves the default and fails verify.
        }
    }
    m.present = (format == "hawk-megammr-backup");
    return m;
}

bool MegaMMRBackup::verifyManifest(const RestoreManifest& zManifest,
                                   MegaMMRBackup& zBackup,
                                   const std::filesystem::path& zPayload,
                                   std::string& zError) {
    if (!zManifest.present) {
        zError = "manifest missing";
        return true;
    }

    RestoreManifest actual = buildManifest(zBackup, zPayload);
    auto fail = [&](const std::string& what) {
        zError = what;
        return false;
    };
    if (actual.sha256 != zManifest.sha256) return fail("sha256 mismatch");
    if (actual.tip_block != zManifest.tip_block) return fail("tip_block mismatch");
    if (!zManifest.tip_hash.empty() && actual.tip_hash != zManifest.tip_hash) {
        return fail("tip_hash mismatch");
    }
    if (!zManifest.weight.empty() && actual.weight != zManifest.weight) {
        return fail("weight mismatch");
    }
    if (actual.first_block != zManifest.first_block) return fail("first_block mismatch");
    if (actual.coins != zManifest.coins) return fail("coin count mismatch");
    if (actual.mmr_entries != zManifest.mmr_entries) return fail("mmr_entries mismatch");
    if (actual.cascade_tip != zManifest.cascade_tip) return fail("cascade_tip mismatch");
    if (actual.tree_root != zManifest.tree_root) return fail("tree_root mismatch");
    if (actual.tree_tip != zManifest.tree_tip) return fail("tree_tip mismatch");
    if (actual.ibd_blocks != zManifest.ibd_blocks) return fail("ibd_blocks mismatch");
    return true;
}

void MegaMMRBackup::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniNumber;

    // Version
    MiniNumber::WriteToStream(out, 1);

    if (!mMegaMMR || !mIBD) {
        throw std::runtime_error("MegaMMRBackup::writeDataStream called with null MegaMMR or IBD");
    }

    // Upcast to Streamable without including dependent headers
    org::minima::utils::Streamable* mmr_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mMegaMMR);
    org::minima::utils::Streamable* ibd_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mIBD);

    // Now the Mega MMR
    mmr_streamable->writeDataStream(out);

    // And the IBD
    ibd_streamable->writeDataStream(out);
}

void MegaMMRBackup::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniNumber;

    int version = MiniNumber::ReadFromStream(in).getAsInt();
    if (version < 1 || version > 8) {
        throw std::runtime_error("Unsupported MegaMMR backup version " + std::to_string(version));
    }

    // Mirror Java: allocate the objects when reading a file.
    if (!mMegaMMR) {
        mOwnedMMR = std::make_unique<org::minima::objects::mmr::MegaMMR>();
        mMegaMMR = mOwnedMMR.get();
    }
    if (!mIBD) {
        mOwnedIBD = std::make_unique<org::minima::objects::IBD>();
        mIBD = mOwnedIBD.get();
    }

    // Upcast to Streamable without including dependent headers
    org::minima::utils::Streamable* mmr_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mMegaMMR);
    org::minima::utils::Streamable* ibd_streamable =
        reinterpret_cast<org::minima::utils::Streamable*>(mIBD);

    // Read into provided instances
    mmr_streamable->readDataStream(in);
    ibd_streamable->readDataStream(in);
}

void MegaMMRBackup::readLegacyMegaMMR(std::istream& in) {
    mOwnedMMR = std::make_unique<org::minima::objects::mmr::MegaMMR>();
    mMegaMMR = mOwnedMMR.get();
    mOwnedIBD.reset();
    mIBD = nullptr;
    mMegaMMR->readDataStream(in);
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org