#include "org/minima/objects/mmr/mega_m_m_r.hpp"

// Full includes for used types (Pitfall 10: include in .cpp)
#include <chrono>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <utility> // <-- FIX: Add for std::move
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"

// Include the full definition for TxPoW to solve incomplete type errors
#include "org/minima/objects/tx_po_w.hpp" 

// MMR related
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/coins/coin_store.hpp"

namespace org {
namespace minima {
namespace objects {
namespace mmr {

using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::Token;
using org::minima::objects::TxBlock;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::params::GeneralParams;
using org::minima::utils::MiniFile;
using org::minima::utils::MiniFormat;
using org::minima::utils::MinimaLogger;

bool MegaMMR::s_PRUNE_LOGS = false;

MegaMMR::MegaMMR()
    : mMMR(std::make_unique<org::minima::objects::mmr::MMR>())
    , mAllUnspentCoins() {
}

MegaMMR::~MegaMMR() = default;
MegaMMR::MegaMMR(MegaMMR&&) noexcept = default;
MegaMMR& MegaMMR::operator=(MegaMMR&&) noexcept = default;

org::minima::objects::mmr::MMR& MegaMMR::getMMR() {
    return *mMMR;
}

const org::minima::objects::mmr::MMR& MegaMMR::getMMR() const {
    return *mMMR;
}

bool MegaMMR::openCoinStore(const std::filesystem::path& zDir) {
    auto store = org::minima::objects::coins::createCoinStore();
    if (!store) return false;
    if (!store->open(zDir)) return false;
    mCoinStore = std::move(store);
    return true;
}

std::size_t MegaMMR::coinCount() const {
    if (mCoinStore) return mCoinStore->size();
    return mAllUnspentCoins.size();
}

void MegaMMR::forEachCoin(const std::function<void(const Coin&)>& fn) const {
    if (mCoinStore) {
        mCoinStore->forEach(fn);
        return;
    }
    for (const auto& kv : mAllUnspentCoins) {
        if (kv.second) fn(*kv.second);
    }
}

void MegaMMR::forEachCoinUntil(const std::function<bool(const Coin&)>& fn) const {
    if (mCoinStore) {
        mCoinStore->forEachUntil(fn);
        return;
    }
    for (const auto& kv : mAllUnspentCoins) {
        if (kv.second && !fn(*kv.second)) break;
    }
}

std::unique_ptr<Coin> MegaMMR::getCoin(const std::string& zCoinID) const {
    if (mCoinStore) return mCoinStore->get(zCoinID);
    auto it = mAllUnspentCoins.find(zCoinID);
    if (it == mAllUnspentCoins.end() || !it->second) return nullptr;
    return it->second->deepCopy();
}

const char* MegaMMR::coinStoreEngine() const {
    return mCoinStore ? mCoinStore->engine() : "memory";
}

std::unordered_map<std::string, std::unique_ptr<Coin>>& MegaMMR::getAllCoins() {
    if (mCoinStore && !mMaterialized) {
        mAllUnspentCoins.clear();
        mCoinStore->forEach([this](const Coin& c) {
            mAllUnspentCoins.emplace(c.getCoinID().to0xString(), c.deepCopy());
        });
        mMaterialized = true;
    }
    return mAllUnspentCoins;
}

const std::unordered_map<std::string, std::unique_ptr<Coin>>& MegaMMR::getAllCoins() const {
    if (mCoinStore && !mMaterialized) {
        mAllUnspentCoins.clear();
        mCoinStore->forEach([this](const Coin& c) {
            mAllUnspentCoins.emplace(c.getCoinID().to0xString(), c.deepCopy());
        });
        mMaterialized = true;
    }
    return mAllUnspentCoins;
}

bool MegaMMR::isEmpty() const {
    return mMMR->getTotalEntries() == 0;
}

void MegaMMR::addBlock(const TxBlock& zBlock) {
    // DEBUG
    // try {
    //     if (!&zBlock) {
    //         MinimaLogger::log("DEBUG MegaMMR::addBlock - zBlock is null reference!");
    //         return;
    //     }
        
    //     // Try to get TxPoW reference
    //     const TxPoW& txpow_ref = zBlock.getTxPoW();
    //     MinimaLogger::log("DEBUG MegaMMR::addBlock - Got TxPoW reference, checking mHeader...");
        
    //     // This will throw if mHeader is null
    //     MiniNumber block = txpow_ref.getBlockNumber();
    //     MinimaLogger::log("DEBUG MegaMMR::addBlock - Block " + block.toString() + " - mHeader is valid!");
        
    // } catch (const std::exception& e) {
    //     MinimaLogger::log("DEBUG MegaMMR::addBlock - EXCEPTION: " + std::string(e.what()));
    //     MinimaLogger::log("DEBUG MegaMMR::addBlock - TxBlock.getTxPoW() returned TxPoW with null mHeader!");
    //     throw;
    // }
    // DEBUG END

    MiniNumber block = zBlock.getTxPoW().getBlockNumber();
    mMMR->setBlockTime(block);

    const auto& peaks = zBlock.getPreviousPeaks();
    for (const auto& peak : peaks) {
        mMMR->setEntry(peak.getRow(), peak.getEntryNumber(), *peak.getMMRData());
    }

    mMMR->calculateEntryNumberFromPeaks();

    const auto& spentcoins = zBlock.getInputCoinProofs();
    for (const auto& input : spentcoins) {
        org::minima::objects::mmr::MMREntryNumber entrynumber = input.getCoin().getMMREntryNumber();

        std::unique_ptr<Coin> spentcoin = input.getCoin().deepCopy();
        spentcoin->setSpent(true);

        // FIX: Use std::move to invoke the move constructor, not the deleted copy constructor
        org::minima::objects::mmr::MMRData mmrdata =
            std::move(*org::minima::objects::mmr::MMRData::CreateMMRDataLeafNode(*spentcoin, MiniNumber::ZERO()));

        mMMR->updateEntry(entrynumber, input.getMMRProof(), mmrdata);

        const std::string spentid = input.getCoin().getCoinID().to0xString();
        if (mCoinStore) mCoinStore->erase(spentid);
        mAllUnspentCoins.erase(spentid);
        mMaterialized = false;
    }

    const auto& outputs = zBlock.getOutputCoins();
    for (const auto& output : outputs) {
        org::minima::objects::mmr::MMREntryNumber entrynumber = mMMR->getEntryNumber();

        std::unique_ptr<Coin> newcoin = output.deepCopy();
        newcoin->setMMREntryNumber(entrynumber);
        newcoin->setBlockCreated(block);
        newcoin->setSpent(false);

        // FIX: Use std::move to invoke the move constructor, not the deleted copy constructor
        org::minima::objects::mmr::MMRData mmrdata =
            std::move(*org::minima::objects::mmr::MMRData::CreateMMRDataLeafNode(*newcoin, newcoin->getAmount()));

        mmrdata.setUnspendable(isPrunable(*newcoin));

        mMMR->addEntry(mmrdata);

        if (mCoinStore) {
            mCoinStore->put(*newcoin);
            mAllUnspentCoins.clear();
            mMaterialized = false;
        } else {
            std::string key = newcoin->getCoinID().to0xString();
            mAllUnspentCoins.emplace(std::move(key), std::move(newcoin));
        }
    }

    MiniData mroot = mMMR->getRoot()->getData();
    MiniData broot = zBlock.getTxPoW().getMMRRoot();
    if (!mroot.isEqual(broot)) {
        MinimaLogger::log("[!] MEGAMMR ROOT AND TXBLOCK ROOT DONT MATCH @ " +
                          zBlock.getTxPoW().getBlockNumber().toString());
    }
}

bool MegaMMR::isPrunable(const Coin& zCoin) const {
    if (GeneralParams::MEGAMMR_MEGAPRUNE) {
        if (zCoin.getAddress().getLength() != 32) {
            return true;
        }
        if (GeneralParams::MEGAMMR_MEGAPRUNE_STATE) {
            if (!zCoin.getState().empty()) {
                return true;
            }
        }
        if (GeneralParams::MEGAMMR_MEGAPRUNE_TOKENS) {
            if (!zCoin.getTokenID().isEqual(Token::TOKENID_MINIMA)) {
                return true;
            }
        }
    }
    return false;
}

void MegaMMR::scanUnspendable() {
    forEachCoin([this](const Coin& cc) {
        org::minima::objects::mmr::MMREntry ment = mMMR->getEntry(0, cc.getMMREntryNumber());

        if (!ment.isEmpty()) {
            auto data = ment.getMMRData();
            data->setUnspendable(isPrunable(cc));
            mMMR->setEntry(ment.getRow(), ment.getEntryNumber(), *data);
        } else {
            MinimaLogger::log(std::string("[!] Coin with no MMREntry in MegaMMR! @ ") + cc.toString());
        }
    });
}

void MegaMMR::pruneUnspendable(bool zScanMMR) {
    auto timestart = std::chrono::steady_clock::now();
    if (s_PRUNE_LOGS) {
        MinimaLogger::log("Start Prune MegaMMR Coins:" + std::to_string(mAllUnspentCoins.size()) +
                          " MMREntries:" + std::to_string(getMMR().getTotalEntries()));
    }

    if (zScanMMR) {
        scanUnspendable();
    }

    mMMR->scanUnspendableTree();

    std::unordered_set<std::string> prunedset;
    const auto& pruned = mMMR->getPrunedUnspendableCoins();
    for (const auto& s : pruned) {
        prunedset.insert(s);
    }

    if (mCoinStore) {
        std::vector<std::string> eraseids;
        mCoinStore->forEach([&](const Coin& c) {
            if (prunedset.find(c.getMMREntryNumber().toString()) != prunedset.end()) {
                eraseids.push_back(c.getCoinID().to0xString());
            }
        });
        for (const auto& id : eraseids) mCoinStore->erase(id);
        mAllUnspentCoins.clear();
        mMaterialized = false;
    } else {
        std::unordered_map<std::string, std::unique_ptr<Coin>> newAllCoins;
        newAllCoins.reserve(mAllUnspentCoins.size());

        for (auto& kv : mAllUnspentCoins) {
            const std::string& key = kv.first;
            std::unique_ptr<Coin>& ccptr = kv.second;

            const org::minima::objects::mmr::MMREntryNumber entry = ccptr->getMMREntryNumber();

            std::string entrystr = entry.toString();
            if (prunedset.find(entrystr) == prunedset.end()) {
                newAllCoins.emplace(key, std::move(ccptr));
            }
        }

        mAllUnspentCoins = std::move(newAllCoins);
    }

    if (s_PRUNE_LOGS) {
        auto timediff = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - timestart)
                            .count();
        MinimaLogger::log("Final Pruned MegaMMR Coins:" + std::to_string(mAllUnspentCoins.size()) +
                          " MMREntries:" + std::to_string(getMMR().getTotalEntries()) +
                          " time:" + std::to_string(timediff) + "ms");
    }
}

void MegaMMR::clear() {
    mMMR = std::make_unique<org::minima::objects::mmr::MMR>();
    mAllUnspentCoins.clear();
    mMaterialized = false;
    if (mCoinStore) mCoinStore->clear();
}

bool MegaMMR::coinCacheMetaMatches(const std::filesystem::path& zRaw) const {
    if (!mCoinStore) return false;
    std::error_code ec;
    if (!std::filesystem::exists(zRaw, ec)) return false;

    std::filesystem::path meta = zRaw;
    meta += ".meta";
    std::ifstream in(meta, std::ios::binary);
    if (!in) return false;

    char magic[8];
    in.read(magic, 8);
    if (!in || std::memcmp(magic, "MMRMETA1", 8) != 0) return false;

    std::int64_t rawsize = 0, rawmtime = 0, coins = 0;
    in.read(reinterpret_cast<char*>(&rawsize), sizeof(rawsize));
    in.read(reinterpret_cast<char*>(&rawmtime), sizeof(rawmtime));
    in.read(reinterpret_cast<char*>(&coins), sizeof(coins));
    if (!in) return false;

    const auto sz = std::filesystem::file_size(zRaw, ec);
    if (ec) return false;
    const auto mt = std::filesystem::last_write_time(zRaw, ec);
    if (ec) return false;
    const std::int64_t mtime = static_cast<std::int64_t>(mt.time_since_epoch().count());

    return static_cast<std::int64_t>(sz) == rawsize &&
           mtime == rawmtime &&
           static_cast<std::int64_t>(mCoinStore->size()) == coins &&
           coins > 0;
}

void MegaMMR::writeCoinCacheMeta(const std::filesystem::path& zRaw) const {
    if (!mCoinStore) return;
    std::error_code ec;
    const auto sz = std::filesystem::file_size(zRaw, ec);
    if (ec) return;
    const auto mt = std::filesystem::last_write_time(zRaw, ec);
    if (ec) return;

    std::filesystem::path meta = zRaw;
    meta += ".meta";
    std::ofstream out(meta, std::ios::binary | std::ios::trunc);
    if (!out) return;

    const char magic[8] = {'M','M','R','M','E','T','A','1'};
    out.write(magic, 8);
    std::int64_t rawsize = static_cast<std::int64_t>(sz);
    std::int64_t rawmtime = static_cast<std::int64_t>(mt.time_since_epoch().count());
    std::int64_t coins = static_cast<std::int64_t>(mCoinStore->size());
    out.write(reinterpret_cast<const char*>(&rawsize), sizeof(rawsize));
    out.write(reinterpret_cast<const char*>(&rawmtime), sizeof(rawmtime));
    out.write(reinterpret_cast<const char*>(&coins), sizeof(coins));
    out.close();
#if !defined(_WIN32)
    ::chmod(meta.c_str(), S_IRUSR | S_IWUSR);
#endif
}

void MegaMMR::loadMMR(const std::filesystem::path& zFile) {
    try {
        auto fsize = std::filesystem::file_size(zFile);
        MinimaLogger::log("Loading MegaMMR size : " + MiniFormat::formatSize(static_cast<long long>(fsize)));
    } catch (...) {
    }
    const bool reuseStore = coinCacheMetaMatches(zFile);
    mSkipCoinLoad = reuseStore;
    MiniFile::loadObjectSlow(zFile, *this);
    mSkipCoinLoad = false;
    if (mCoinStore) {
        // A full load (re)populated the store; record the fingerprint for the
        // next start. A reused store already matches and needs no rewrite.
        if (!reuseStore) writeCoinCacheMeta(zFile);
        mCoinStore->sync();
    }
#if !defined(_WIN32)
    // The MegaMMR holds the full UTXO/coin set - owner-only.
    ::chmod(zFile.c_str(), S_IRUSR | S_IWUSR);
#endif
}

void MegaMMR::saveMMR(const std::filesystem::path& zFile) {
    MiniFile::saveObjectDirect(zFile, *this);
    if (mCoinStore) {
        writeCoinCacheMeta(zFile);
        mCoinStore->sync();
    }
#if !defined(_WIN32)
    ::chmod(zFile.c_str(), S_IRUSR | S_IWUSR);
#endif
    if (s_PRUNE_LOGS) {
        try {
            auto fsize = std::filesystem::file_size(zFile);
            MinimaLogger::log("Saving MegaMMR size : " + MiniFormat::formatSize(static_cast<long long>(fsize)));
        } catch (...) {
        }
    }
}

void MegaMMR::writeDataStream(std::ostream& zOut) {
    if (GeneralParams::MEGAMMR_MEGAPRUNE) {
        pruneUnspendable(false);
    }

    MiniNumber::WriteToStream(zOut, 1);
    mMMR->writeDataStream(zOut);

    MiniNumber::WriteToStream(zOut, static_cast<int>(coinCount()));

    forEachCoin([&](const Coin& c) {
        const_cast<Coin&>(c).writeDataStream(zOut);
    });
}

void MegaMMR::readDataStream(std::istream& zIn) {
    int version = MiniNumber::ReadFromStream(zIn).getAsInt();
    (void)version; 

    mMMR = std::make_unique<org::minima::objects::mmr::MMR>();
    mMMR->readDataStream(zIn);
    mMMR->setFinalized(false);

    const int size = MiniNumber::ReadFromStream(zIn).getAsInt();

    // Fast restart: the MMR is loaded and the LMDB store already holds exactly
    // this many coins (fingerprint matched) - skip the coin payload entirely.
    if (mSkipCoinLoad && mCoinStore &&
        mCoinStore->size() == static_cast<std::size_t>(size)) {
        return;
    }

    mAllUnspentCoins.clear();
    mMaterialized = false;
    if (mCoinStore) mCoinStore->clear();

    if (mCoinStore) {
        std::vector<std::shared_ptr<Coin>> batch;
        batch.reserve(10000);
        for (int i = 0; i < size; ++i) {
            auto cc = Coin::ReadFromStream(zIn);
            if (!cc) continue;
            batch.emplace_back(std::shared_ptr<Coin>(std::move(cc)));
            if (batch.size() >= 10000) {
                mCoinStore->putBatch(batch);
                batch.clear();
            }
        }
        if (!batch.empty()) mCoinStore->putBatch(batch);
    } else {
        for (int i = 0; i < size; ++i) {
            auto cc = Coin::ReadFromStream(zIn);
            if (!cc) continue;
            mAllUnspentCoins.emplace(cc->getCoinID().to0xString(), std::move(cc));
        }
    }

    if (GeneralParams::MEGAMMR_MEGAPRUNE) {
        pruneUnspendable(true);
    } else {
        scanUnspendable();
    }
}

org::minima::objects::mmr::MMRData MegaMMR::getCoinData() {
    return getCoinData(MiniNumber::ZERO());
}

org::minima::objects::mmr::MMRData
MegaMMR::getCoinData(const MiniNumber& zNumber) {
    Coin test(org::minima::objects::base::MiniData::ZERO_TXPOWID(), zNumber,
              org::minima::objects::base::MiniData::ZERO_TXPOWID());

    // FIX: Use std::move to invoke the move constructor for the return value
    return std::move(*org::minima::objects::mmr::MMRData::CreateMMRDataLeafNode(test, zNumber));
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org
