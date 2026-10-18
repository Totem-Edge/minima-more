#include "org/minima/objects/mmr/mega_m_m_r.hpp"

// Full includes for used types (Pitfall 10: include in .cpp)
#include <chrono>
#include <unordered_set>
#include <filesystem>
#include <utility> // <-- FIX: Add for std::move

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

std::unordered_map<std::string, std::unique_ptr<Coin>>& MegaMMR::getAllCoins() {
    return mAllUnspentCoins;
}

const std::unordered_map<std::string, std::unique_ptr<Coin>>& MegaMMR::getAllCoins() const {
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

        mAllUnspentCoins.erase(input.getCoin().getCoinID().to0xString());
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

        std::string key = newcoin->getCoinID().to0xString();
        mAllUnspentCoins.emplace(std::move(key), std::move(newcoin));
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
    for (auto& kv : mAllUnspentCoins) {
        Coin& cc = *kv.second;

        org::minima::objects::mmr::MMREntry ment = mMMR->getEntry(0, cc.getMMREntryNumber());

        if (!ment.isEmpty()) {
            auto data = ment.getMMRData();
            data->setUnspendable(isPrunable(cc));
            mMMR->setEntry(ment.getRow(), ment.getEntryNumber(), *data);
        } else {
            MinimaLogger::log(std::string("[!] Coin with no MMREntry in MegaMMR! @ ") + cc.toString());
        }
    }
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
}

void MegaMMR::loadMMR(const std::filesystem::path& zFile) {
    try {
        auto fsize = std::filesystem::file_size(zFile);
        MinimaLogger::log("Loading MegaMMR size : " + MiniFormat::formatSize(static_cast<long long>(fsize)));
    } catch (...) {
    }
    MiniFile::loadObjectSlow(zFile, *this);
}

void MegaMMR::saveMMR(const std::filesystem::path& zFile) {
    MiniFile::saveObjectDirect(zFile, *this);
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

    int size = static_cast<int>(mAllUnspentCoins.size());
    MiniNumber::WriteToStream(zOut, size);

    for (auto& kv : mAllUnspentCoins) {
        Coin& cc = *kv.second;
        cc.writeDataStream(zOut);
    }
}

void MegaMMR::readDataStream(std::istream& zIn) {
    int version = MiniNumber::ReadFromStream(zIn).getAsInt();
    (void)version; 

    mMMR = std::make_unique<org::minima::objects::mmr::MMR>();
    mMMR->readDataStream(zIn);
    mMMR->setFinalized(false);

    mAllUnspentCoins.clear();
    int size = MiniNumber::ReadFromStream(zIn).getAsInt();
    for (int i = 0; i < size; ++i) {
        auto cc = Coin::ReadFromStream(zIn);
        mAllUnspentCoins.emplace(cc->getCoinID().to0xString(), std::move(cc));
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
