#include "org/minima/system/commands/backup/mmrsync/megammr.hpp"

#include <fstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <system_error>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/main.hpp"

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/mini_util.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Headers assumed to exist in the project (same package)
#include "org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.hpp"

using org::minima::system::commands::CommandException;

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

megammr::megammr()
    : org::minima::system::commands::Command("megammr", "(action:) (file:) - Get Info on or Import / Export the MegaMMR data") {}

std::string megammr::getFullHelp() const {
    return std::string("\nmegammr\n"
                       "\n"
                       "View information about your MegaMMR. Export and Import complete MegaMMR data.\n"
                       "\n"
                       "You must be running -megammr.\n"
                       "\n"
                       "action: (optional)\n"
                       "    info   : Shows info about your MegaMMR.\n"
                       "    export : Export a MegaMMR data file.\n"
                       "    import : Import a MegaMMR data file.\n"
                       "\n"
                       "file: (optional)\n"
                       "    Use with export and import.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "megammr\n"
                       "\n"
                       "megammr action:export\n"
                       "\n"
                       "megammr action:export file:thefile\n"
                       "\n"
                       "megammr action:import file:thefile\n");
}

std::vector<std::string> megammr::getValidParams() const {
    return std::vector<std::string>{ "action", "file" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> megammr::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::objects::mmr::MegaMMR;
    using org::minima::objects::IBD;
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::MiniFile;
    using org::minima::utils::MiniFormat;
    using org::minima::system::params::GeneralParams;

    auto ret = getJSONReply();

    std::string action = getParam("action", "info");

    MegaMMR& megammrdb = MinimaDB::getDB()->getMegaMMR();

    if (action == "info") {
        JSONObject resp;
        resp.put("enabled", GeneralParams::IS_MEGAMMR);
        resp.put("mmr", megammrdb.getMMR().toJSON(false));
        resp.put("coins", megammrdb.getAllCoins().size());

        ret->put("response", resp);
        return ret;
    } else if (action == "export") {
        if (!GeneralParams::IS_MEGAMMR) {
            throw CommandException("MegaMMR not enabled");
        }

        std::string file = getParam("file", "");
        if (file.empty()) {
            // "megammr_"+DATE+".megammr"
            std::string date = org::minima::utils::MiniUtil::DATEFORMAT().format(std::time(nullptr));
            file = "megammr_" + date + ".megammr";
        }

        std::filesystem::path backupfile = MiniFile::createBaseFile(file);

        if (std::filesystem::exists(backupfile)) {
            std::error_code ec;
            std::filesystem::remove(backupfile, ec);
        }

        IBD ibd;

        // Lock DB for read
        MinimaDB::getDB()->readLock(true);

        try {
            // Build complete IBD snapshot
            ibd.createCompleteIBD();

            // Create backup object and write it to file (expects pointers)
            org::minima::system::commands::backup::mmrsync::MegaMMRBackup mmrbackup(&megammrdb, &ibd);

            std::ofstream ofs(backupfile, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!ofs) {
                MinimaDB::getDB()->readLock(false);
                throw CommandException(std::string("Failed to open backup file for writing: ") + backupfile.string());
            }

            mmrbackup.writeDataStream(ofs);
            ofs.flush();
            ofs.close();

            // Unlock DB
            MinimaDB::getDB()->readLock(false);

            JSONObject resp;
            resp.put("megammrtip", megammrdb.getMMR().getBlockTime());
            resp.put("ibdtip", ibd.getTreeTip());
            resp.put("backup", std::filesystem::absolute(backupfile).string());
            resp.put("size", MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(backupfile))));

            ret->put("response", resp);
            return ret;
        } catch (const std::exception& exc) {
            // Unlock DB on error
            MinimaDB::getDB()->readLock(false);
            throw CommandException(exc.what());
        }
    } else if (action == "import") {
        if (!GeneralParams::IS_MEGAMMR) {
            throw CommandException("MegaMMR not enabled");
        }

        std::string file = getParam("file", "");
        if (file.empty()) {
            throw CommandException("MUST specify a file to restore from");
        }

        std::filesystem::path restorefile = MiniFile::createBaseFile(file);
        if (!std::filesystem::exists(restorefile)) {
            throw CommandException(std::string("Restore file doesn't exist : ") + std::filesystem::absolute(restorefile).string());
        }

        org::minima::system::commands::backup::mmrsync::MegaMMRBackup mmrback;

        try {
            org::minima::utils::MinimaLogger::log(std::string("Loading MegaMMR.. size:") +
                                                  org::minima::utils::MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(restorefile))));
            MiniFile::loadObjectSlow(restorefile, mmrback);
        } catch (const std::exception& exc) {
            throw CommandException(exc.what());
        }

        // Reset archive state and install MegaMMR into DB
        org::minima::system::Main::getInstance()->archiveResetReady(false);

        MinimaDB::getDB()->getMegaMMR().clear();
        // mmrback.getMegaMMR() returns a pointer; hardSetMegaMMR expects a reference
        MinimaDB::getDB()->hardSetMegaMMR(*mmrback.getMegaMMR());

        // Respond to caller (we cannot call TxPoWProcessor methods without their headers)
        JSONObject resp;
        resp.put("message", std::string("MegaMMR import finished.. please restart"));
        ret->put("response", resp);

        // Perform shutdown sequence using available APIs
        org::minima::system::Main::getInstance()->setHasShutDown();
        org::minima::database::MinimaDB::getDB()->saveAllDB();
        org::minima::system::Main::getInstance()->shutdown();
        org::minima::system::Main::getInstance()->NotifyMainListenerOfShutDown();

        return ret;
    } else if (action == "integrity") {
        std::string file = getParam("file");

        std::filesystem::path restorefile = MiniFile::createBaseFile(file);
        if (!std::filesystem::exists(restorefile)) {
            throw CommandException(std::string("MegaMMR file doesn't exist : ") + std::filesystem::absolute(restorefile).string());
        }

        org::minima::system::commands::backup::mmrsync::MegaMMRBackup mmrback;

        org::minima::utils::MinimaLogger::log(std::string("Load MegaMMR.. ") +
                                              org::minima::utils::MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(restorefile))));
        MiniFile::loadObjectSlow(restorefile, mmrback);

        auto weight = checkMegaMMR(mmrback);

        // Build response with available IBD info
        JSONObject resp;
        auto ibd = mmrback.getIBD();
        resp.put("chaintip", ibd->getTreeTip());
        resp.put("weight", weight.convert_to<std::string>());

        ret->put("response", resp);
        return ret;
    }

    // Default: return reply if action unrecognized
    return ret;
}

org::minima::system::commands::Command* megammr::getFunction() {
    return new megammr();
}

boost::multiprecision::cpp_int megammr::checkMegaMMR(const std::filesystem::path& zMegaMMR) {
    using org::minima::utils::MiniFile;
    using org::minima::utils::MiniFormat;
    using org::minima::utils::MinimaLogger;

    org::minima::system::commands::backup::mmrsync::MegaMMRBackup mmrback;

    MinimaLogger::log(std::string("Load MegaMMR.. ") +
                      MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(zMegaMMR))));
    MiniFile::loadObjectSlow(zMegaMMR, mmrback);

    return checkMegaMMR(mmrback);
}

boost::multiprecision::cpp_int megammr::checkMegaMMR(org::minima::system::commands::backup::mmrsync::MegaMMRBackup& mmrback) {
    using org::minima::utils::MinimaLogger;
    using org::minima::objects::mmr::MegaMMR;
    using org::minima::objects::mmr::MMR;
    using org::minima::objects::TxBlock;
    using org::minima::objects::Coin;
    using org::minima::objects::mmr::MMRData;
    using org::minima::objects::mmr::MMRProof;
    using org::minima::objects::base::MiniNumber;

    // Get MMR and MegaMMR
    MegaMMR* mega = mmrback.getMegaMMR();
    MMR& mmr = mega->getMMR();

    // Validate IBD
    auto ibd = mmrback.getIBD();
    MinimaLogger::log("Check IBD..");
    bool validibd = ibd->checkValidData();
    if (!validibd) {
        throw CommandException("Invalid IBD");
    }

    // Start/end block as per tip of the MMR
    MiniNumber lastblock = mmr.getBlockTime();

    // Load IBD blocks into the MegaMMR
    auto& blocks = ibd->getTxBlocks();
    for (const auto& blocksptr : blocks) {
        const TxBlock& block = *blocksptr;

        MiniNumber blknum = block.getTxPoW().getBlockNumber();
        if (!blknum.isEqual(lastblock.increment())) {
            throw CommandException(std::string("Invalid block number.. not incremental.. last_in_mega:") +
                                   lastblock.toString() + " new_block:" + blknum.toString());
        }

        // Update
        lastblock = blknum;

        // Add to MegaMMR
        mega->addBlock(block);
    }

    // Finalize
    mmr.finalizeSet();

    MinimaLogger::log("Now check all coin proofs..");

    // Check integrity of all coins
    auto& allcoins = mega->getAllCoins();
    int size = static_cast<int>(allcoins.size());

    int maxcheck = 0;
    for (auto& kv : allcoins) {
        Coin& coin = *kv.second;

        // Create MMRData leaf node for coin
        std::unique_ptr<MMRData> mmrdata = MMRData::CreateMMRDataLeafNode(coin, coin.getAmount());
        MMRProof mmrproof;

        try {
            // Get the proof
            mmrproof = mmr.getProof(coin.getMMREntryNumber());
        } catch (const std::exception& exc) {
            throw CommandException(std::string("Error chcking coin @ ") + coin.toJSON().toString() + " " + exc.what());
        }

        // Now check the proof
        bool valid = mmr.checkProofTimeValid(coin.getMMREntryNumber(), *mmrdata, std::move(mmrproof));
        if (!valid) {
            throw CommandException(std::string("INVALID Coin proof! @ ") + coin.toJSON().toString());
        }

        maxcheck++;
        if (maxcheck % 5000 == 0) {
            MinimaLogger::log(std::string("Checking coins @ ") + std::to_string(maxcheck) + " / " + std::to_string(size));
        }
    }

    MinimaLogger::log(std::string("All coins checked ") + std::to_string(maxcheck) + " / " + std::to_string(size));

    // Return total weight
    return ibd->getTotalWeight();
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org