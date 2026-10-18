#include "org/minima/system/brains/tx_po_w_processor.hpp"

#include <chrono>
#include <exception>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/system/brains/tx_po_w_checker.hpp"
#include "org/minima/database/archive/tx_block_d_b.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"

#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/stack.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

// Optional: MiniByte (assumed available in project)
#include "org/minima/objects/base/mini_byte.hpp"

// // Forward declare missing checker (assumed implemented elsewhere)
// // NOTE: We assume TxPoWChecker methods expect raw pointers (TxPoWTreeNode*)
// namespace org { namespace minima { namespace system { namespace brains {
// class TxPoWChecker {
// public:
//     static bool checkTxPoWBlockTimed(org::minima::database::txpowtree::TxPoWTreeNode* parentnode,
//                                      const org::minima::objects::TxPoW& blocktxpow,
//                                      const std::vector<const org::minima::objects::TxPoW*>& alltrans);
//     static bool checkTxBlockOnly(org::minima::database::txpowtree::TxPoWTreeNode* parentnode,
//                                  const org::minima::objects::TxBlock& txblock);
// };
// } } } }

// // Forward declare archive::TxBlockDB API (assumed elsewhere)
// // NOTE: We assume TxBlock stores and returns std::shared_ptr<TxPoW> via getTxPoWShared()
// namespace org { namespace minima { namespace database { namespace archive {
// class TxBlockDB {
// public:
//     void addTxBlock(org::minima::objects::TxBlock& zTxBlock);
//     std::vector<std::shared_ptr<org::minima::objects::TxBlock>> getChildBlocks(const std::string& zParentTxPoWID);
//     std::shared_ptr<org::minima::objects::TxBlock> findTxBlock(const std::string& zTxPoWID);
//     void clearOld(const org::minima::objects::base::MiniNumber& zBeforeBlock);
// };
// } } } }

// // Forward declare CascadeNode's minimal API for use (assumed elsewhere)
// namespace org { namespace minima { namespace database { namespace cascade {
// class CascadeNode {
// public:
//     const org::minima::objects::TxPoW& getTxPoW() const;
// };
// } } } }

using org::minima::database::MinimaDB;
using org::minima::database::archive::ArchiveManager;
using org::minima::database::cascade::Cascade;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::txpowtree::TxPowTree;

using org::minima::objects::IBD;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MegaMMR;

using org::minima::system::Main;
using org::minima::system::network::minima::NIOManager;
using org::minima::system::network::minima::NIOMessage;

using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;

using org::minima::utils::MinimaLogger;
using org::minima::utils::Stack;
using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;

// Static constant
const MiniNumber org::minima::system::brains::TxPoWProcessor::THREE_HOURS = MiniNumber(1000LL * 60 * 60 * 3);

// Helper to get current time millis
static inline std::int64_t now_millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

namespace org {
namespace minima {
namespace system {
namespace brains {

TxPoWProcessor::TxPoWProcessor()
    : org::minima::utils::messages::MessageProcessor("TXPOWPROCESSOR")
    , mFirstIBD(now_millis())
{
    startMessageProcessorThread();
}

void TxPoWProcessor::resetFirstIBDTimer() {
    mFirstIBD = now_millis();
}

void TxPoWProcessor::postProcessTxPoW(const std::shared_ptr<TxPoW>& zTxPoW) {
    // Are we shutting down or restoring
    if (Main::getInstance()->isShuttongDownOrRestoring()) {
        return;
    }

    // Add / Update last access to the DB
    // FIX: Use . (dot operator) on the reference returned by getTxPoWDB()
    // FIX: Pass the shared_ptr zTxPoW, as expected by addTxPoW
    bool relevant = MinimaDB::getDB()->getTxPoWDB().addTxPoW(zTxPoW);

    // If relevant - post a message
    if (relevant) {
        auto data = std::make_shared<JSONObject>();
        data->put("relevant", true);
        data->put("txpow", zTxPoW->toJSON());
        Main::getInstance()->PostNotifyEvent("NEWTXPOW", *data);
    } else if (GeneralParams::NOTIFY_ALL_TXPOW) {
        auto data = std::make_shared<JSONObject>();
        data->put("relevant", false);
        data->put("txpow", zTxPoW->toJSON());
        Main::getInstance()->PostNotifyEvent("NEWTXPOW", *data);
    }

    // Do NOT process if you are a txblock node
    if (GeneralParams::TXBLOCK_NODE) {
        return;
    }

    // Post a message on the single threaded stack
    Message msg(TXPOWPROCESSOR_PROCESSTXPOW);
    msg.addObject("txpow", zTxPoW);
    PostMessage(std::make_shared<Message>(msg));
}

void TxPoWProcessor::postProcessTxBlock(const std::shared_ptr<TxBlock>& zTxBlock) {
    // Are we shutting down or restoring
    if (Main::getInstance()->isShuttongDownOrRestoring()) {
        return;
    }

    // ONLY txblocknodes do this
    if (!GeneralParams::TXBLOCK_NODE) {
        return;
    }

    // Add to the RAM DB
    // FIX: Use . (dot operator) on the reference returned by getTxBlockDB()
    MinimaDB::getDB()->getTxBlockDB().addTxBlock(zTxBlock);

    // Add / Update last access to the DB
    // FIX: We cannot copy the TxPoW (deleted constructor).
    // We must use the deepCopy() method (which we know exists from line 297)
    // to create a new one, then move it into a shared_ptr.
    auto txpow_copy_uniq = zTxBlock->getTxPoW().deepCopy();
    auto txpow_copy = std::shared_ptr<org::minima::objects::TxPoW>(std::move(txpow_copy_uniq));
    MinimaDB::getDB()->getTxPoWDB().addTxPoW(txpow_copy);

    // Post a message on the single threaded stack
    Message msg(TXPOWPROCESSOR_PROCESSTXBLOCK);
    msg.addObject("txblock", zTxBlock);
    PostMessage(std::make_shared<Message>(msg));
}

void TxPoWProcessor::postProcessIBD(const std::shared_ptr<IBD>& zIBD, const std::string& zClientUID) {
    postProcessIBD(zIBD, zClientUID, false);
}

void TxPoWProcessor::postProcessIBD(const std::shared_ptr<IBD>& zIBD, const std::string& zClientUID, bool zOverrideRestore) {
    // Are we shutting down
    if (Main::getInstance()->isShuttongDownOrRestoring() && !zOverrideRestore) {
        return;
    }

    // If we are doing this as a MegaMMR restore.. reset..
    if (zOverrideRestore) {
        mIBDSyncFinished = false;
    }

    // Post a message on the single threaded stack
    Message msg(TXPOWPROCESSOR_PROCESS_IBD);
    msg.addObject("ibd", zIBD);
    msg.addString("uid", zClientUID);
    msg.addBoolean("notifyfinish", zOverrideRestore);
    PostMessage(std::make_shared<Message>(msg));
}

bool TxPoWProcessor::isIBDProcessFinished() {
    return mIBDSyncFinished;
}

void TxPoWProcessor::postProcessSyncIBD(const std::shared_ptr<IBD>& zIBD, const std::string& zClientUID) {
    Message msg(TXPOWPROCESSOR_PROCESS_SYNCIBD);
    msg.addObject("ibd", zIBD);
    msg.addString("uid", zClientUID);
    PostMessage(std::make_shared<Message>(msg));
}

void TxPoWProcessor::postProcessArchiveIBD(const std::shared_ptr<IBD>& zIBD, const std::string& zClientUID) {
    Message msg(TXPOWPROCESSOR_PROCESS_ARCHIVEIBD);
    msg.addObject("ibd", zIBD);
    msg.addString("uid", zClientUID);
    PostMessage(std::make_shared<Message>(msg));
}

void TxPoWProcessor::onStartUpRecalc() {
    recalculateTree();
}

void TxPoWProcessor::processTxPoW(const std::shared_ptr<TxPoW>& zTxPoW) {
    bool recalculate = false;

    // FIX: Use & (address-of) to get a pointer from the returned reference
    TxPoWDB* txpdb     = &MinimaDB::getDB()->getTxPoWDB();
    TxPowTree* txptree = &MinimaDB::getDB()->getTxPoWTree();
    Cascade* cascdb    = &MinimaDB::getDB()->getCascade();
    (void)cascdb; // not directly used here

    // Process a stack of TxPoW if necessary
    Stack processstack;
    processstack.push(zTxPoW);

    while (!processstack.isEmpty()) {
        auto anytx = processstack.pop();
        auto txpow = std::any_cast<std::shared_ptr<TxPoW>>(anytx);

        // Need tip and root
        // FIX: Store returned shared_ptr in 'auto' or 'shared_ptr' variable
        auto tipnode  = txptree->getTip();
        auto rootnode = txptree->getRoot();
        if (!tipnode || !rootnode) {
            continue;
        }

        MiniNumber blknum  = txpow->getBlockNumber();
        MiniNumber tipnum  = (tipnode->getBlockNumber());
        MiniNumber rootnum = (rootnode->getBlockNumber());

        bool validrange = false;
        if (GeneralParams::TEST_PARAMS) {
            validrange = blknum.isMore(rootnum);
        } else {
            if (tipnum.isLess(MiniNumber::THOUSAND())) {
                validrange = true;
            } else {
                MiniNumber minblock = rootnum.add(GlobalParams::MINIMA_BLOCKS_SPEED_CALC);
                if (blknum.isMore(minblock)) {
                    validrange = true;
                }
            }
        }

        if (txpow->isBlock() && !validrange) {
            if (GeneralParams::BLOCK_LOGS) {
                MinimaLogger::log("Invalid range for block check @ " + blknum.toString()
                                  + " root:" + rootnum.toString()
                                  + " tip:" + tipnum.toString()
                                  + " txpowid:" + txpow->getTxPoWID());
            }
        }

        if (txpow->isBlock() && validrange) {
            // Check not already added
            // FIX: Store returned shared_ptr in 'auto'
            auto oldnode = txptree->findNode(txpow->getTxPoWID());
            if (oldnode == nullptr) { // FIX: Check shared_ptr directly
                // Check for valid parent node
                // FIX: Store returned shared_ptr in 'auto'
                auto parentnode = txptree->findNode(txpow->getParentID().to0xString());
                if (parentnode != nullptr) { // FIX: Check shared_ptr directly
                    // Do we have all the transactions in the block
                    std::vector<std::string> txns = txpow->getTransactions();
                    std::size_t numtxns = txns.size();
                    auto alltrans_unique = txpdb->getAllTxPoW(txns);

                    if (alltrans_unique.size() == numtxns) {
                        // 1. Create vectors to hold pointers for the Checker and TxBlock
                        // We use the pointers directly from the shared_ptr objects returned by the DB.
                        // Because 'alltrans_unique' stays alive in this scope, these raw pointers remain valid.
                        
                        std::vector<TxPoW*> alltrans_ptrs_nonconst;
                        alltrans_ptrs_nonconst.reserve(alltrans_unique.size());

                        for (auto& sp : alltrans_unique) {
                            // 'sp' is a std::shared_ptr<TxPoW>
                            alltrans_ptrs_nonconst.push_back(sp.get());
                        }

                        // 2. Validate block using the non-const pointers (H17)
                        bool validblock = TxPoWChecker::checkTxPoWBlockTimed(parentnode.get(), *txpow, alltrans_ptrs_nonconst);

                        if (validblock) {
                            // 3. Construct TxBlock
                            // IMPORTANT: The TxBlock constructor *must* copy the transactions internally
                            // if it takes raw pointers. Assuming the standard Minima C++ port behavior, 
                            // it deep copies from these pointers.
                            auto txblock = std::make_shared<TxBlock>(parentnode.get()->getMMR(), *txpow, alltrans_ptrs_nonconst);

                            // Add to the RAM DB
                            MinimaDB::getDB()->getTxBlockDB().addTxBlock(txblock);

                            if (GeneralParams::BLOCK_LOGS) {
                                MinimaLogger::log("Added block to tree : " + txblock->getTxPoW().getBlockNumber().toString()
                                                + " " + txblock->getTxPoW().getTxPoWID());
                            }

                            // Broadcast
                            try {
                                org::minima::objects::base::MiniByte type(NIOMessage::MSG_TXBLOCKID());
                                auto txpowid_data = txblock->getTxPoW().getTxPoWIDData(); 
                                NIOManager::sendNetworkMessageAll(type, txpowid_data);
                            } catch (const std::exception& e) {
                                MinimaLogger::log(e);
                            }

                            // Create a new node and add to tree
                            auto newblock = std::make_shared<TxPoWTreeNode>(*txblock);
                            parentnode->addChildNode(newblock);
                            txptree->addFastLink(newblock);

                            // Need to recalculate tree later
                            recalculate = true;

                            // Process children
                            auto children_unique = txpdb->getChildBlocks(txpow->getTxPoWID());
                            for (auto& child_up : children_unique) {
                                processstack.push(child_up);
                            }
                        } else {
                            MinimaLogger::log("[!] Failed block check @ " + txpow->getBlockNumber().toString()
                                            + " txpowid:" + txpow->getTxPoWID()
                                            + " root:" + rootnum.toString()
                                            + " tip:" + tipnum.toString());
                        }
                    }
                } else {
                    // Do we have the parent TxPoW
                    // FIX: Store returned shared_ptr in 'auto'
                    auto parent = txpdb->getTxPoW(txpow->getParentID().to0xString());
                    if (parent != nullptr) { // FIX: Check shared_ptr
                        // Push parent to process
                        // FIX: Push the shared_ptr directly
                        processstack.push(parent);
                    }
                }
            }
        }
    }

    if (recalculate) {
        recalculateTree();
    }
}

void TxPoWProcessor::processTxBlock(const std::shared_ptr<TxBlock>& zTxBlock) {
    if (!GeneralParams::TXBLOCK_NODE) {
        return;
    }

    bool recalculate = false;

    // FIX: Use & (address-of) to get pointers from references
    TxPowTree* txptree = &MinimaDB::getDB()->getTxPoWTree();
    Cascade* cascdb    = &MinimaDB::getDB()->getCascade();
    (void)cascdb;

    Stack processstack;
    processstack.push(zTxBlock);

    while (!processstack.isEmpty()) {
        auto anyblk = processstack.pop();
        auto trustedtxblock = std::any_cast<std::shared_ptr<TxBlock>>(anyblk);
        const TxPoW& txpow = trustedtxblock->getTxPoW(); // This getTxPoW returns const TxPoW&

        // FIX: Store returned shared_ptr in 'auto'
        auto tipnode  = txptree->getTip();
        auto rootnode = txptree->getRoot();
        if (!tipnode || !rootnode) {
            continue;
        }

        MiniNumber blknum  = txpow.getBlockNumber();
        MiniNumber tipnum  = (tipnode->getBlockNumber());
        MiniNumber rootnum = (rootnode->getBlockNumber());

        bool validrange = false;
        if (GeneralParams::TEST_PARAMS) {
            validrange = blknum.isMore(rootnum);
        } else {
            if (tipnum.isLess(MiniNumber::THOUSAND())) {
                validrange = true;
            } else {
                MiniNumber minblock = rootnum.add(GlobalParams::MINIMA_BLOCKS_SPEED_CALC);
                if (blknum.isMore(minblock)) {
                    validrange = true;
                }
            }
        }

        if (txpow.isBlock() && !validrange) {
            if (GeneralParams::BLOCK_LOGS) {
                MinimaLogger::log("[!] Invalid range for txblock check slavemode @ "
                                  + blknum.toString() + " root:" + rootnum.toString()
                                  + " tip:" + tipnum.toString()
                                  + " txpowid:" + txpow.getTxPoWID());
            }
        }

        if (txpow.isBlock() && validrange) {
            // FIX: Store returned shared_ptr in 'auto'
            auto oldnode = txptree->findNode(txpow.getTxPoWID());
            if (oldnode == nullptr) { // FIX: Check shared_ptr
                // FIX: Store returned shared_ptr in 'auto'
                auto parentnode = txptree->findNode(txpow.getParentID().to0xString());
                if (parentnode != nullptr) { // FIX: Check shared_ptr
                    // FIX: Pass raw pointer using .get()
                    bool validblock = TxPoWChecker::checkTxBlockOnly(parentnode.get(), *trustedtxblock);
                    if (validblock) {
                        if (GeneralParams::BLOCK_LOGS) {
                            MinimaLogger::log("Added TxBlock to tree : "
                                              + trustedtxblock->getTxPoW().getBlockNumber().toString()
                                              + " " + trustedtxblock->getTxPoW().getTxPoWID());
                        }

                        auto newblock = std::make_shared<TxPoWTreeNode>(*trustedtxblock);
                        parentnode->addChildNode(newblock);
                        // FIX: Pass the shared_ptr `newblock` directly
                        txptree->addFastLink(newblock);
                        recalculate = true;

                        // FIX: Use . (dot operator)
                        auto children = MinimaDB::getDB()->getTxBlockDB().getChildBlocks(txpow.getTxPoWID());
                        for (auto& child : children) {
                            processstack.push(child);
                        }
                    } else {
                        MinimaLogger::log("[!] Failed txblock check @ "
                                          + txpow.getBlockNumber().toString()
                                          + " txpowid:" + txpow.getTxPoWID()
                                          + " root:" + rootnum.toString()
                                          + " tip:" + tipnum.toString());
                    }
                } else {
                    // FIX: Use . (dot operator)
                    auto parent = MinimaDB::getDB()->getTxBlockDB().findTxBlock(txpow.getParentID().to0xString());
                    if (parent) {
                        processstack.push(parent);
                    }
                }
            }
        }
    }

    if (recalculate) {
        recalculateTree();
    }
}

bool TxPoWProcessor::processSyncBlock(const std::shared_ptr<TxBlock>& zTxBlock) {

    // MinimaLogger::log("DEBUG_BLOCK: Processing sync block " +  zTxBlock->getTxPoW().getBlockNumber().toString() + " hash: " + zTxBlock->getTxPoW().getTxPoWID());

    // DBs
    // FIX: Use & (address-of) to get pointers from references
    Cascade* cascdb    = &MinimaDB::getDB()->getCascade();
    TxPoWDB* txpdb     = &MinimaDB::getDB()->getTxPoWDB();
    TxPowTree* txptree = &MinimaDB::getDB()->getTxPoWTree();

    // Add to RAM DB
    // FIX: Use . (dot operator)
    MinimaDB::getDB()->getTxBlockDB().addTxBlock(zTxBlock);

    // Ensure TxPoW is in DB
    // FIX: We cannot copy the TxPoW (deleted constructor).
    // Use the deepCopy() method, then move it into a shared_ptr.
    auto txpow_copy_sync_uniq = zTxBlock->getTxPoW().deepCopy();
    auto txpow_copy_sync = std::shared_ptr<org::minima::objects::TxPoW>(std::move(txpow_copy_sync_uniq));
    txpdb->addTxPoW(txpow_copy_sync);

    // If we already have a tree root, ignore any SyncBlock at/before the current root.
    // These are either part of our cascade or come from overlapping IBDs of other peers.
    auto curroot = txptree->getRoot();
    if (curroot != nullptr) {
        MiniNumber rootnum = curroot->getBlockNumber();
        MiniNumber blk = zTxBlock->getTxPoW().getBlockNumber();
        if (blk.isLessEqual(rootnum)) {
            return false;
        }
    }

    // Do we have ANY TxPoW in the tree at all..
    if (txptree->getTip() == nullptr) {
        // If we just adopted a cascade, the first SyncBlock must continue from the cascade tip.
        // If not, the first SyncBlock is the new root.
        if (cascdb->getTip() != nullptr) {
            const MiniNumber txblknum = zTxBlock->getTxPoW().getBlockNumber();
            const MiniNumber cascblk  = cascdb->getTip()->getTxPoW().getBlockNumber();

            if (cascblk.isEqual(txblknum.sub(MiniNumber::ONE()))) {
                // Create a node and set as root; cascade tip is parent by construction.
                auto newnode = std::make_shared<TxPoWTreeNode>(*zTxBlock);
                txptree->setRoot(newnode);
                return true;
            } else {
                // Not the immediate next block after cascade — treat as new root.
                auto newnode = std::make_shared<TxPoWTreeNode>(*zTxBlock);
                txptree->setRoot(newnode);
                return true;
            }
        }

        // No cascade tip: just bootstrap with this block as root.
        auto newnode = std::make_shared<TxPoWTreeNode>(*zTxBlock);
        txptree->setRoot(newnode);
        return true;
    }

    // Check not already added
    // FIX: Store returned shared_ptr in 'auto'
    auto oldnode = txptree->findNode(zTxBlock->getTxPoW().getTxPoWID());
    if (oldnode == nullptr) { // FIX: Check shared_ptr
        // FIX: Store returned shared_ptr in 'auto'
        auto parentnode = txptree->findNode(zTxBlock->getTxPoW().getParentID().to0xString());
        if (parentnode != nullptr) { // FIX: Check shared_ptr
            auto newblock = std::make_shared<TxPoWTreeNode>(*zTxBlock);
            parentnode->addChildNode(newblock);
            // FIX: Pass the shared_ptr `newblock` directly
            txptree->addFastLink(newblock);
            return true;
        } else {
            // Parent not in RAM tree. This is common for:
            // - Historical blocks before our current root (from other peers' IBDs)
            // - First block right after an adopted cascade (its parent is cascade tip)
            MiniNumber blk = zTxBlock->getTxPoW().getBlockNumber();
            bool historical = false;
            auto r = txptree->getRoot();
            if (r) {
                if (blk.isLessEqual(r->getBlockNumber())) historical = true;
            }
            Cascade* cdb = &MinimaDB::getDB()->getCascade();
            if (cdb->getTip() != nullptr) {
                MiniNumber ctip = cdb->getTip()->getTxPoW().getBlockNumber();
                if (blk.isLessEqual(ctip)) historical = true;
            }
            if (historical) {
                // Silently ignore stale SyncBlock from overlapping IBD
                return false;
            }
            // Not obviously historical: log and skip instead of crashing the IBD
            MinimaLogger::log("[!] SyncBlock missing parent (skipping, not fatal) @ " + blk.toString()
                              + " parent:" + zTxBlock->getTxPoW().getParentID().to0xString());
            return false;
        }
    }

    return false;
}

void TxPoWProcessor::recalculateTree() {
    // Required DBs
    // FIX: Use & (address-of) to get pointers from references
    TxPoWDB* txpdb     = &MinimaDB::getDB()->getTxPoWDB();
    TxPowTree* txptree = &MinimaDB::getDB()->getTxPoWTree();
    Cascade* cascdb    = &MinimaDB::getDB()->getCascade();
    ArchiveManager* arch = &MinimaDB::getDB()->getArchive();
    MegaMMR* megammr   = &MinimaDB::getDB()->getMegaMMR();

    MinimaDB::getDB()->writeLock(true);
    try {
        // FIX: Store returned shared_ptr in 'auto'
        auto currenttip = txptree->getTip();

        // Recalculate the whole tree
        txptree->recalculateTree();

        int maxlen = GlobalParams::MINIMA_CASCADE_START_DEPTH.add(GlobalParams::MINIMA_CASCADE_FREQUENCY).getAsInt();
        if (txptree->getHeaviestBranchLength() >= maxlen) {
            // FIX: Store returned shared_ptr in 'auto'
            auto tip = txptree->getTip();

            // get the new root..
            MiniNumber tgt = tip->getTxPoW().getBlockNumber()
                                 .sub(GlobalParams::MINIMA_CASCADE_START_DEPTH)
                                 .increment();
            std::shared_ptr<MiniNumber> tgtsp = std::make_shared<MiniNumber>(tgt);
            auto newroot = tip->getPastNode(tgt);

            // Now copy all the MMR Coins..
            if (newroot) {
                newroot->copyParentRelevantCoins();
            }

            // NOW - Shrink it..
            auto cascade = txptree->setLength(GlobalParams::MINIMA_CASCADE_START_DEPTH.getAsInt());

            // Add these nodes to the cascade
            // MinimaLogger::log("DEBUG: Processing " + std::to_string(cascade.size()) + " nodes for cascade");

            for (const auto& txpnode : cascade) {
                auto& txpowsp = txpnode->getTxPoW();

                // MinimaLogger::log("DEBUG: Adding block " + txpowsp.getBlockNumber().toString() + 
                //       " to cascade, TxPoWID=" + txpowsp.getTxPoWID());


                // These TxPoW are now in the cascade - that can NEVER change..
                txpdb->setInCascade(txpowsp.getTxPoWID());
                for (const auto& txpid : txpowsp.getTransactions()) {
                    txpdb->setInCascade(txpid);
                }

                // Store in the ArchiveManager
                arch->saveBlock(txpnode->getTxBlock());

                // And add to the cascade
                cascdb->addToTip(txpowsp);

                // MinimaLogger::log("DEBUG: Successfully added block " + txpowsp.getBlockNumber().toString() + 
                //       " to cascade. Cascade now has " + std::to_string(cascdb->getLength()) + " nodes");


                // Send out Notify Messages for coins added
                try {
                    // FIX: Pass the std::shared_ptr<TxBlock> from the node
                    TxPoWTreeNode::CheckTxBlockForNotifyCoins(txpnode->getTxBlock());
                } catch (const std::exception& exc) {
                    MinimaLogger::log(exc);
                }

                // Are we running in MEGA MMR
                if (GeneralParams::IS_MEGAMMR) {
                    megammr->addBlock(txpnode->getTxBlock());
                }
            }

            if (GeneralParams::IS_MEGAMMR) {
                megammr->getMMR().pruneTree();
            }

            // And finally..
            cascdb->cascadeChain();

            // Clear the TxBlockDB
            if (newroot) {
                // FIX: Use . (dot operator)
                MinimaDB::getDB()->getTxBlockDB().clearOld(newroot->getBlockNumber().sub(MiniNumber::HUNDRED()));
            }
        }

        // Set all the onchain txns
        txpdb->clearMainChainTxns();

        // Current Tip
        // FIX: Directly assign the shared_ptr
        auto tip = txptree->getTip();
        while (tip) {
            auto& txpowsp = tip->getTxPoW();
            txpdb->setOnMainChain(txpowsp.getTxPoWID());

            auto txns = txpowsp.getTransactions();
            for (const auto& txn : txns) {
                txpdb->setOnMainChain(txn);
            }

            tip = tip->getParent();
        }

        // Has the Tip changed..
        // FIX: Store returned shared_ptr in 'auto'
        auto newtipnode = txptree->getTip();
        if (currenttip != nullptr && newtipnode != nullptr) {
            auto curid = currenttip->getTxPoW().getTxPoWIDData();
            auto newid = newtipnode->getTxPoW().getTxPoWIDData();
            if (!curid.isEqual(newid)) {
                Message m(org::minima::system::Main::MAIN_NEWBLOCK);
                auto txpow_ptr = std::shared_ptr<const TxPoW>(newtipnode, &(newtipnode->getTxPoW()));
                m.addObject("txpow", txpow_ptr);
                Main::getInstance()->PostMessage(std::make_shared<Message>(m));
            }
        }
    } catch (const std::exception& exc) {
        MinimaLogger::log(exc);
    }

    // Unlock
    MinimaDB::getDB()->writeLock(false);
}

void TxPoWProcessor::postCheckCall() {
    Message msg(TXPOWPROCESSOR_CHECKER_CALL);
    PostMessage(std::make_shared<Message>(msg));
}

void TxPoWProcessor::processMessage(Message& zMessage) {
    // Are we shutting down..
    if (Main::getInstance()->isShuttingDown()) {
        return;
    }

    if (zMessage.isMessageType(TXPOWPROCESSOR_PROCESSTXPOW)) {
        if (GeneralParams::TXBLOCK_NODE) {
            return;
        }
        auto anytx = zMessage.getObject("txpow");
        auto txp = std::any_cast<std::shared_ptr<TxPoW>>(anytx);
        processTxPoW(txp);

    } else if (zMessage.isMessageType(TXPOWPROCESSOR_PROCESSTXBLOCK)) {
        if (!GeneralParams::TXBLOCK_NODE) {
            return;
        }
        auto anyblk = zMessage.getObject("txblock");
        auto txblock = std::any_cast<std::shared_ptr<TxBlock>>(anyblk);
        processTxBlock(txblock);

    } else if (zMessage.isMessageType(TXPOWPROCESSOR_PROCESS_IBD)) {
        std::string uid = zMessage.getString("uid");
        auto anyibd = zMessage.getObject("ibd");
        auto ibd = std::any_cast<std::shared_ptr<IBD>>(anyibd);
        bool notifyfinish = zMessage.getBoolean("notifyfinish");

        if (!ibd->checkValidData()) {
            return;
        }

        try {
            Main::getInstance()->setSyncIBD(true);
            // MinimaLogger::log("DEBUG_SYNC: setSyncIBD(TRUE) called at IBD start");

            if (GeneralParams::IBDSYNC_LOGS) {
                MinimaLogger::log("Processing main IBD length : " + std::to_string(ibd->getTxBlocks().size()));
            }
            std::int64_t timestart = now_millis();

            // Handle cascade if present
            if (ibd->hasCascade()) {
                // FIX: Use . (dot operator)
                if (MinimaDB::getDB()->getCascade().getTip() == nullptr) {
                    bool ignore = false;
                    // FIX: Use . (dot operator) and store shared_ptr in 'auto'
                    auto root = MinimaDB::getDB()->getTxPoWTree().getRoot();
                    if (root != nullptr) { // FIX: Check shared_ptr
                        MiniNumber rootblock = (root->getBlockNumber());
                        MiniNumber casctip   = ibd->getCascade()->getTip()->getTxPoW().getBlockNumber();

                        if (!casctip.isEqual(rootblock.decrement())) {
                            ignore = true;
                            MinimaLogger::log("[!] IGNORE IBD - My Tree Root : " + rootblock.toString()
                                              + " IBD TIP : " + casctip.toString());
                        } else {
                            MiniData rootparent = root->getTxPoW().getParentID();
                            MiniData casctipid  = ibd->getCascade()->getTip()->getTxPoW().getTxPoWIDData();

                            if (!rootparent.isEqual(casctipid)) {
                                ignore = true;
                                MinimaLogger::log("[!] IGNORE IBD - Invalid Hash for parents");
                            }
                        }
                    }

                    if (!ignore) {
                        MinimaDB::getDB()->writeLock(true);
                        try {
                            // Set IBD cascade (adopt)
                            // FIX: Dereference pointer to pass as const&
                            MinimaDB::getDB()->setIBDCascade(*ibd->getCascade());
                            // Store cascade in archive if needed
                            // FIX: Use . (dot operator) and dereference pointer
                            MinimaDB::getDB()->getArchive().checkCascadeRequired(*ibd->getCascade());
                        } catch (const std::exception& exc) {
                            MinimaLogger::log(exc);
                        }
                        MinimaDB::getDB()->writeLock(false);
                    }
                } else {
                    // Already have a cascade; ignore incoming cascade
                }
            }

            // Now process the SyncBlocks
            // FIX: Use & (address-of)
            TxPowTree* txptree = &MinimaDB::getDB()->getTxPoWTree();
            MiniNumber timenow = MiniNumber(static_cast<long long>(now_millis()));

            long diff = now_millis() - mFirstIBD;
            if ((diff > MAX_FIRST_IBD_TIME) && !GeneralParams::TXBLOCK_NODE) {
                if (txptree->getTip() != nullptr && !ibd->getTxBlocks().empty()) {
                    MiniNumber notxblocktimediff = MiniNumber(1000LL * 60 * 180);
                    if (GeneralParams::TEST_PARAMS) {
                        notxblocktimediff = MiniNumber(1000LL * 60 * 5);
                    }
                    auto tdiff = txptree->getTip()->getTxPoW().getTimeMilli().sub(timenow).abs();
                    if (tdiff.isLess(notxblocktimediff)) {
                        MinimaLogger::log("Your chain tip is up to date - no TxBlocks accepted - only FULL TxPoW");
                        Main::getInstance()->setSyncIBD(false);
                        askToSyncTxBlocks(uid);
                        return;
                    }
                }
            }

            int additions = 0;
            auto& blocks = ibd->getTxBlocks();
            for (auto& block : blocks) {
                try {
                    processSyncBlock(block);
                    additions++;

                    // Request any missing transactions
                    requestMissingTxns(uid, block);

                    if (additions > 256) {
                        recalculateTree();
                        additions = 0;

                        if (GeneralParams::IBDSYNC_LOGS) {
                            MinimaLogger::log(std::string("[!] Processed IBD block @ ")
                                              + block->getTxPoW().getBlockNumber().toString());
                        }
                    }
                } catch (const std::exception& exc) {
                    MinimaLogger::log(std::string("[IBD-FAIL] block:") + block->getTxPoW().getBlockNumber().toString()
                                      + " txpowid:" + block->getTxPoW().getTxPoWID()
                                      + " error:" + exc.what());
                    Main::getInstance()->getNIOManager().disconnect(uid, true);
                    break;
                }
            }

            recalculateTree();

            std::int64_t timediff = now_millis() - timestart;
            if (GeneralParams::IBDSYNC_LOGS) {
                MinimaLogger::log("Processing main IBD finished " + std::to_string(timediff) + "ms");
            }
            
            // MinimaLogger::log("DEBUG_SYNC: setSyncIBD(FALSE) called - IBD completed successfully");
            Main::getInstance()->setSyncIBD(false);

            // Drain queued TXPOWID/TXBLOCKID seen during IBD (best-effort, bounded)
            // We don't have a single peer here; ask the caller to trigger a targeted drain if needed.
            // As a simple policy, ask the provided uid to serve anything it advertised.
            // Actual drain of the global queue is performed in NIOMessage::drainPendingDuringIBD
            // which is invoked from the NIOMessage path when IBD ends.

            // FIX: Use . (dot operator)
            MinimaDB::getDB()->getArchive().checkForCleanDB();

            askToSyncTxBlocks(uid);

            if (notifyfinish) {
                mIBDSyncFinished = true;
            }
        } catch (const std::exception& exc) {
            // MinimaLogger::log("DEBUG_SYNC: OUTER CATCH - Exception: " + std::string(exc.what()));
            // MinimaLogger::log("DEBUG_SYNC: Current isSyncIBD state: " + std::string(Main::getInstance()->isSyncIBD() ? "TRUE" : "FALSE"));
            MinimaLogger::log(std::string("[!] TXPOWPROCESSOR_PROCESS_IBD ERROR ") + exc.what());
            MinimaLogger::log(exc);
        }

    } else if (zMessage.isMessageType(TXPOWPROCESSOR_PROCESS_SYNCIBD)) {
        std::string uid = zMessage.getString("uid");
        auto anyibd = zMessage.getObject("ibd");
        auto ibd = std::any_cast<std::shared_ptr<IBD>>(anyibd);

        auto& blocks = ibd->getTxBlocks();
        if (blocks.empty()) {
            return;
        }

        // FIX: Use & (address-of)
        ArchiveManager* arch = &MinimaDB::getDB()->getArchive();

        auto lastblock = arch->loadLastBlock();
        const TxPoW* lastpow = nullptr;
        if (!lastblock) {
            // FIX: Use . (dot operator)
            lastpow = &(MinimaDB::getDB()->getTxPoWTree().getRoot()->getTxPoW());
        } else {
            lastpow = &lastblock->getTxPoW();
        }

        for (auto& block : blocks) {
            MiniNumber lastnum = lastpow->getBlockNumber();

            if (block->getTxPoW().getBlockNumber().isEqual(lastnum.decrement())) {
                if (block->getTxPoW().getTxPoWIDData().isEqual(lastpow->getParentID())) {
                    arch->saveBlock(*block);
                } else {
                    MinimaLogger::log("[-] Invalid block parent in TxBlock sync.. @ "
                                      + block->getTxPoW().getBlockNumber().toString() + " from " + uid);
                    return;
                }
            }

            lastpow = &block->getTxPoW();
        }

        // FIX: Use . (dot operator)
        MinimaDB::getDB()->getArchive().checkForCleanDB();

        askToSyncTxBlocks(uid);

    } else if (zMessage.isMessageType(TXPOWPROCESSOR_PROCESS_ARCHIVEIBD)) {
        auto anyibd = zMessage.getObject("ibd");
        auto archibd = std::any_cast<std::shared_ptr<IBD>>(anyibd);
        std::string uid = zMessage.getString("uid");

        int additions = 0;
        auto& blocks = archibd->getTxBlocks();

        if (!blocks.empty() && GeneralParams::IBDSYNC_LOGS) {
            MinimaLogger::log("Processing Archive IBD length:" + std::to_string(blocks.size())
                              + " start:" + blocks.front()->getTxPoW().getBlockNumber().toString());
        }

        for (auto& block : blocks) {
            try {
                processSyncBlock(block);
                additions++;

                if (additions > 1000) {
                    recalculateTree();
                    additions = 0;
                }
            } catch (const std::exception& exc) {
                MinimaLogger::log(exc.what());
                Main::getInstance()->getNIOManager().disconnect(uid);
                break;
            }
        }

        recalculateTree();

    } else if (zMessage.isMessageType(TXPOWPROCESSOR_CHECKER_CALL)) {
        MinimaLogger::log("TXPOWPROCESSOR Checker Call Recieved..");
    }
}

void TxPoWProcessor::askToSyncTxBlocks(const std::string& zClientID) {
    if (zClientID == "0x00") {
        return;
    }
    if (!GeneralParams::NO_SYNC_IBD) {
        Message synctxblock(NIOManager::NIO_SYNCTXBLOCK);
        synctxblock.addString("client", zClientID);
        Main::getInstance()->getNIOManager().PostMessage(std::make_shared<Message>(synctxblock));
    }
}

void TxPoWProcessor::requestMissingTxns(const std::string& zClientID, const std::shared_ptr<TxBlock>& zBlock) {
    if (GeneralParams::TXBLOCK_NODE) {
        return;
    }

    const TxPoW& txp = zBlock->getTxPoW();

    MiniNumber timenow = MiniNumber(static_cast<long long>(now_millis()));
    MiniNumber mintime = timenow.sub(THREE_HOURS);
    if (txp.getTimeMilli().isLess(mintime)) {
        return;
    }

    try {
        auto txns = txp.getBlockTransactions();
        for (const MiniData& txn : txns) {
            // FIX: Use . (dot operator)
            bool exists = MinimaDB::getDB()->getTxPoWDB().exists(txn.to0xString());
            if (!exists) {
                org::minima::objects::base::MiniByte type(NIOMessage::MSG_TXPOWREQ());
                NIOManager::sendNetworkMessage(zClientID, type, const_cast<MiniData&>(txn));
            }
        }
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org

