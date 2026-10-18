#include "org/minima/system/commands/base/status.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <any> // Include for std::any
#include <memory> // Include for shared_ptr

#ifdef _WIN32
  // #define NOMINMAX // Already defined by build system or std lib headers
  #include <windows.h>
  #include <psapi.h>
#elif defined(__APPLE__)
  #include <mach/mach.h>
#else
  #include <unistd.h> // For sysconf
#endif

// Force include MMR dependencies known to cause PIMPL issues
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
// FIX 1: Ensure correct include name
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/cascade/cascade.hpp"
// FIX 4: Add full include for CascadeNode
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/archive/archive_manager.hpp"

#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace fs = std::filesystem;

// Anonymous namespace for helpers remains unchanged
namespace {
    // ... getProcessMemoryBytesPortable, getFileSizeBytesSafe, formatDoubleAsString ...
static std::uint64_t getProcessMemoryBytesPortable() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<std::uint64_t>(pmc.WorkingSetSize);
    } return 0;
#elif defined(__APPLE__)
    task_basic_info_data_t tinfo{};
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (KERN_SUCCESS == task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&tinfo), &count)) {
        return static_cast<std::uint64_t>(tinfo.resident_size);
    } return 0;
#else // Linux/POSIX
    std::ifstream statm("/proc/self/statm");
    if (statm) {
        long resident_pages = 0;
        if (statm >> resident_pages >> resident_pages) { // Read first (ignored), then RSS
            long page_size = sysconf(_SC_PAGESIZE);
            if (page_size > 0) {
                return static_cast<std::uint64_t>(resident_pages) * static_cast<std::uint64_t>(page_size);
            }
        }
    } return 0;
#endif
}
// FIX: Corrected function signature
static std::uintmax_t getFileSizeBytesSafe(const std::string& path) {
    std::error_code ec;
    std::uintmax_t size = fs::file_size(path, ec);
    return ec ? 0 : size;
}
static std::string formatDoubleAsString(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << v;
    return oss.str();
}
static std::string formatWeightAsString(double v) {
    if (v == 0.0) {
        return "0";
    }
    return formatDoubleAsString(v);
}
} // anonymous namespace


namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

status::status()
    : org::minima::system::commands::Command(
          "status",
          "(clean:true) - Show general status for Minima and clean RAM") {}

std::string status::getFullHelp() const {
     return std::string("\nstatus\n" /* ... rest of help ... */ "\n");
}

std::vector<std::string> status::getValidParams() const {
    return std::vector<std::string>{ "clean", "debug", "complete" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> status::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::cascade::Cascade;
    using org::minima::database::cascade::CascadeNode; // Now defined
    using org::minima::database::txpowdb::TxPoWDB;
    // FIX 1: Ensure correct type name is used
    using org::minima::database::txpowtree::TxPowTree;
    using org::minima::database::txpowtree::TxPoWTreeNode;
    using org::minima::database::wallet::Wallet;
    using org::minima::database::archive::ArchiveManager;
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::TxPoW;
    using org::minima::system::Main;
    using org::minima::system::params::GeneralParams;
    using org::minima::system::params::GlobalParams;
    using org::minima::system::network::NetworkManager;
    using org::minima::utils::MiniFile;
    using org::minima::utils::MiniFormat;
    using org::minima::utils::MinimaLogger;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    if (existsParam("clean")) {
        MinimaLogger::log("RAM Clean requested (no-op in C++).");
    }

    bool debug = getBooleanParam("debug", false);
    bool complete = getBooleanParam("complete", false);

    //
    // FIX 1: Change from pointers (*) to references (&)
    //
    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
    TxPowTree& txptree = MinimaDB::getDB()->getTxPoWTree();
    Cascade& cascade = MinimaDB::getDB()->getCascade();
    Wallet& wallet = MinimaDB::getDB()->getWallet();
    ArchiveManager& arch = MinimaDB::getDB()->getArchive();

    JSONObject details;
    details.put("version", GlobalParams::getFullMicroVersion());
    details.put("uptime", MiniFormat::ConvertMilliToTime(Main::getInstance()->getUptimeMilli()));
    
    //
    // FIX 2: Use dot (.) operator on references, not arrow (->)
    //
    details.put("locked", !wallet.isBaseSeedAvailable());

    if (complete) {
        bool normalmode = Main::getInstance()->isNormalMineMode();
        details.put("powmode", normalmode ? std::string("normal") : std::string("low"));
    }

    double chainweight = 0.0;
    double cascweight = 0.0;
    
    //
    // FIX 2: Use dot (.) operator on references
    //
    if (txptree.getTip() != nullptr) {
        long long totallength = static_cast<long long>(txptree.getHeaviestBranchLength() + cascade.getLength());
        details.put("length", totallength);

        if (txptree.getRoot() != nullptr) {
            chainweight = txptree.getRoot()->getTotalWeight().getAsDouble();
        }
        cascweight = cascade.getTotalWeight().getAsDouble();
        details.put("weight", formatWeightAsString(chainweight + cascweight));

        const TxPoW& tip_txpow = txptree.getTip()->getTxPoW();
        const auto& tip_mmr = txptree.getTip()->getMMR();

        details.put("minima", tip_txpow.getMMRTotal().toString());
        details.put("coins", tip_mmr.getEntryNumber().toString());

    } else {
        details.put("length", 0LL);
        details.put("weight", std::string("0"));
        details.put("minima", std::string("0"));
        details.put("coins", std::string("0"));
    }

    details.put("data", GeneralParams::DATA_FOLDER);

    if (debug) MinimaLogger::log("Main Settings Done..");

    JSONObject files;
    std::uint64_t mem = getProcessMemoryBytesPortable();
    files.put("ram", MiniFormat::formatSize(static_cast<long long>(mem)));
    std::uintmax_t allfiles = MiniFile::getTotalFileSize(fs::path(GeneralParams::DATA_FOLDER));
    files.put("disk", MiniFormat::formatSize(static_cast<long long>(allfiles)));

    JSONObject database;
    
    //
    // FIX 2 & 3: Use dot (.) operator and .string() for path
    //
    std::uintmax_t txpdbsz = getFileSizeBytesSafe(txpdb.getSqlFile().string());
    database.put("txpowdb", MiniFormat::formatSize(static_cast<long long>(txpdbsz)));
   
    std::uintmax_t archsz = getFileSizeBytesSafe(arch.getSQLFile());
    database.put("archivedb", MiniFormat::formatSize(static_cast<long long>(archsz)));
    database.put("cascade", MiniFormat::formatSize(static_cast<long long>(MinimaDB::getDB()->getCascadeFileSize())));
    database.put("chaintree", MiniFormat::formatSize(static_cast<long long>(MinimaDB::getDB()->getTxPowTreeFileSize())));
    std::uintmax_t walletsz = getFileSizeBytesSafe(wallet.getSQLFile());
    database.put("wallet", MiniFormat::formatSize(static_cast<long long>(walletsz)));
    database.put("userdb", MiniFormat::formatSize(static_cast<long long>(MinimaDB::getDB()->getUserDBFileSize())));
    database.put("p2pdb", MiniFormat::formatSize(static_cast<long long>(MinimaDB::getDB()->getP2PFileSize())));

    if (complete) {
        JSONObject allthefiles;
        MiniFile::getTotalFileSizeWithNames(fs::path(GeneralParams::DATA_FOLDER), allthefiles, 3, 0);
        database.put("allfiles", std::any(allthefiles));
    }

    files.put("files", std::any(database));
    details.put("memory", std::any(files));

    if (debug) MinimaLogger::log("Memory Done..");

    JSONObject tree;
    
    //
    // FIX 2: Use dot (.) operator
    //
    if (txptree.getRoot() != nullptr) {
        if (auto tip = txptree.getTip()) {
            const auto& tip_txpow = tip->getTxPoW();
        
            tree.put("block", tip_txpow.getBlockNumber().getAsLong());
            tree.put("time", tip_txpow.getTimeMilli().toString());
            tree.put("hash", tip_txpow.getTxPoWID());
            tree.put("difficulty", tip_txpow.getBlockDifficulty().to0xString());

            if (tip_txpow.getBlockNumber().isLessEqual(MiniNumber::TWO())) {
                tree.put("speed", "1.0");
            } else {
                
                //
                // FIX 4: Just copy the shared_ptr, don't try to construct a new one
                //
                std::shared_ptr<TxPoWTreeNode> treestartblock_sptr = tip;

                // parent_sptr is already a shared_ptr, which is what we need
                std::shared_ptr<TxPoWTreeNode> parent_sptr = treestartblock_sptr->getParent(GlobalParams::MINIMA_BLOCKS_SPEED_CALC.getAsInt());

                if (treestartblock_sptr && parent_sptr) {
                // Store the returned shared_ptr in a shared_ptr, and pass the shared_ptr argument
                std::shared_ptr<TxPoWTreeNode> startblock = org::minima::system::brains::TxPoWGenerator::getMedianTimeBlock(treestartblock_sptr);
                std::shared_ptr<TxPoWTreeNode> endblock = org::minima::system::brains::TxPoWGenerator::getMedianTimeBlock(parent_sptr);

                if (startblock && endblock) {
                    // FIX 3: Dereference the MiniNumber shared_ptrs before calling sub
                    // (Assuming getBlockNumber returns shared_ptr based on error message)
                    auto start_num_ptr = startblock->getBlockNumber();
                    auto end_num_ptr = endblock->getBlockNumber();
                    auto start_num = startblock->getBlockNumber();
                    auto end_num = endblock->getBlockNumber();
                    MiniNumber blockdiff = start_num.sub(end_num); // Dereference both
                    if (blockdiff.isEqual(MiniNumber::ZERO())) {
                        tree.put("speed", MiniNumber::MINUSONE().toString());
                    } else {
                        auto speed = org::minima::system::brains::TxPoWGenerator::getChainSpeed(startblock, blockdiff);
                        tree.put("speed", speed.setSignificantDigits(5).toString());
                    }
                    
                } else {
                    tree.put("speed", MiniNumber::MINUSONE().toString());
                }
                } else {
                    tree.put("speed", MiniNumber::MINUSONE().toString());
                }
            }
        } else {
            tree.put("block", 0LL);
            tree.put("time", std::string("N/A"));
            tree.put("hash", std::string("N/A"));
            tree.put("difficulty", std::string("N/A"));
            tree.put("speed", MiniNumber::MINUSONE().toString());
        }

        //
        // FIX 2: Use dot (.) operator
        //
        tree.put("size", txptree.getSize());
        tree.put("length", txptree.getHeaviestBranchLength());
        tree.put("branches", txptree.getSize() - txptree.getHeaviestBranchLength());
        tree.put("weight", formatWeightAsString(chainweight));
    } else {
        tree.put("block", 0LL);
        tree.put("time", std::string("NO BLOCKS YET"));
        tree.put("hash", std::string("0x00"));
        tree.put("length", 0);
        tree.put("branches", 0);
    }

    if (debug) MinimaLogger::log("Chain Tree done..");

    JSONObject casc;
    //
    // FIX 2: Use dot (.) operator
    //
    if (cascade.getTip() != nullptr) {
        // Now we can access methods on the CascadeNode tip
        casc.put("start", cascade.getTip()->getTxPoW().getBlockNumber().getAsLong());
    } else {
        casc.put("start", -1);
    }
    casc.put("length", cascade.getLength());
    casc.put("weight", formatWeightAsString(cascweight));
    tree.put("cascade", std::any(casc));

    details.put("chain", std::any(tree));

    JSONObject dbstats;
    
    //
    // FIX 2: Use dot (.) operator
    //
    dbstats.put("mempool", static_cast<int>(txpdb.getAllUnusedTxns().size()));
    if (debug) MinimaLogger::log("Mempool done..");
    dbstats.put("ramdb", txpdb.getRamSize());
    if (debug) MinimaLogger::log("RamDB done..");
    dbstats.put("txpowdb", txpdb.getSqlSize());
    if (debug) MinimaLogger::log("txpowdb done..");
    
    dbstats.put("archivedb", arch.getSize());
    if (debug) MinimaLogger::log("archivedb done..");

    details.put("txpow", std::any(dbstats));

    if (debug) MinimaLogger::log("Database done..");

    NetworkManager* netmanager = &Main::getInstance()->getNetworkManager();
    if (netmanager != nullptr) {
        //
        // FIX 5: getStatus() returns a JSONObject, not a pointer.
        // Do not check with 'if' or dereference with '*'.
        //
        auto netstat = netmanager->getStatus();
        details.put("network", std::any(netstat));
    } else {
         details.put("network", std::any(JSONObject()));
    }

    if (debug) MinimaLogger::log("Network done..");

    ret->put("response", std::any(details));
    return ret;
}

org::minima::system::commands::Command* status::getFunction() {
    return new status();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org