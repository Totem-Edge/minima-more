#include "org/minima/objects/i_b_d.hpp"

#include <algorithm>
#include <unordered_set>
#include <iostream>
#include <cmath>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/greeting.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"

using org::minima::system::params::GeneralParams;

namespace org {
namespace minima {
namespace objects {

// Static constant
const org::minima::objects::base::MiniNumber IBD::MAX_BLOCKS_FOR_IBD = org::minima::objects::base::MiniNumber(34000);

// Constructor
IBD::IBD() : mCascade(nullptr), mTxBlocks() {}

// Destructor and move ops (PIMPL-FIX)
IBD::~IBD() = default;
IBD::IBD(IBD&&) noexcept = default;
IBD& IBD::operator=(IBD&&) noexcept = default;

// Accessors
org::minima::objects::base::MiniNumber IBD::getTreeRoot() const {
    // Assumes at least one block exists (as in Java)
    return mTxBlocks.at(0)->getTxPoW().getBlockNumber();
}

org::minima::objects::base::MiniNumber IBD::getTreeTip() const {
    int len = static_cast<int>(mTxBlocks.size());
    return mTxBlocks.at(len - 1)->getTxPoW().getBlockNumber();
}

// Builders
bool IBD::createIBD(const org::minima::objects::Greeting& zGreeting) {
    using org::minima::database::MinimaDB;
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::MinimaLogger;

    auto& txptree = MinimaDB::getDB()->getTxPoWTree();

    bool isvalid = true;

    if (txptree.getTip() == nullptr) {
        // We have nothing.. !
        return isvalid;
    }

    // Lock DB
    MinimaDB::getDB()->readLock(true);

    try {
        MiniNumber greettip  = zGreeting.getTopBlock();
        MiniNumber greetroot = zGreeting.getRootBlock();

        bool fresh = greettip.isEqual(MiniNumber::MINUSONE());

        if (fresh) {
            createCompleteIBD();

        } else {
            MiniNumber myroot = txptree.getRoot()->getTxBlock().getTxPoW().getBlockNumber();
            auto tip = txptree.getTip();
            MiniNumber mytip = tip->getTxBlock().getTxPoW().getBlockNumber();

            if (greettip.isLess(myroot)) {
                // Their chain is behind our cascade.. will need to send Archived Sync Blocks AND the full chain

                MiniNumber found = MiniNumber::MINUSONE();
                int counter = 0;

                const auto& chain = zGreeting.getChain();
                for (const auto& current : chain) {
                    if (counter % 20 == 0) {
                        //
                        // FIX: Use dot '.' operator on ArchiveManager reference
                        //
                        found = MinimaDB::getDB()->getArchive().exists(current->to0xString());
                        if (!found.isEqual(MiniNumber::MINUSONE())) {
                            break;
                        }
                    }
                    counter++;
                }

                if (found.isEqual(MiniNumber::MINUSONE())) {
                    int size = static_cast<int>(chain.size());
                    if (size > 0) {
                        //
                        // FIX: Use dot '.' operator on ArchiveManager reference
                        //
                        found = MinimaDB::getDB()->getArchive().exists(chain[size - 1]->to0xString());
                    }
                }

                if (!found.isEqual(MiniNumber::MINUSONE())) {
                    bool toobig = false;
                    MiniNumber total = myroot.sub(found);
                    if (total.isMore(IBD::MAX_BLOCKS_FOR_IBD)) {
                        toobig = true;
                    }

                    if (!toobig) {
                        // Add the whole tree first
                        while (tip != nullptr) {
                            // Insert at start to maintain order from root to tip
                            mTxBlocks.insert(mTxBlocks.begin(), std::shared_ptr<TxBlock>(tip, &(tip->getTxBlock())));
                            tip = tip->getParent();
                        }

                        //
                        // FIX: Use dot '.' operator on ArchiveManager reference
                        //
                        auto blocks = MinimaDB::getDB()->getArchive().loadBlockRange(found, myroot);
                        for (auto& blk : blocks) {
                            mTxBlocks.insert(mTxBlocks.begin(), std::shared_ptr<TxBlock>(std::move(blk)));
                        }

                    } else {
                        MinimaLogger::log("Intersection found but User too far back to sync.. too many blocks:" +
                                          total.toString() + " max:" + IBD::MAX_BLOCKS_FOR_IBD.toString());
                        createCompleteIBD();
                    }
                } else {
                    MinimaLogger::log("No Archive blocks found to match New User.. ");
                    createCompleteIBD();
                }

            } else if (greetroot.isMore(mytip)) {
                MinimaLogger::log("We are Too old to sync new user! greetroot" + greetroot.toString() +
                                  " mytip:" + mytip.toString());
                createCompleteIBD();

            } else {
                bool found = false;
                std::string foundblockID;

                // Greeting blocks (hex strings of TxPoWIDs or block IDs)
                std::vector<std::string> greetblocks;
                greetblocks.reserve(zGreeting.getChain().size());
                for (const auto& block : zGreeting.getChain()) {
                    greetblocks.push_back(block->to0xString());
                }

                // Our blocks set
                std::unordered_set<std::string> myblocks;
                while (tip != nullptr) {
                    myblocks.insert(tip->getTxPoW().getTxPoWID());
                    tip = tip->getParent();
                }

                // Intersect
                for (const auto& block : greetblocks) {
                    if (myblocks.find(block) != myblocks.end()) {
                        found = true;
                        foundblockID = block;
                        break;
                    }
                }

                if (found) {
                    tip = MinimaDB::getDB()->getTxPoWTree().getTip();
                    while (tip != nullptr) {
                        std::string currentid = tip->getTxPoW().getTxPoWID();

                        if (currentid != foundblockID) {
                            mTxBlocks.insert(mTxBlocks.begin(), std::shared_ptr<TxBlock>(tip, &(tip->getTxBlock())));
                        } else {
                            break;
                        }

                        tip = tip->getParent();
                    }
                }else {
                    // DEBUG
                    MinimaLogger::log("[!] When creating IBD - No Crossover found whilst syncing with new node. "
                                    "They are on a different chain. Please check you are on the correct chain");
                    
                    // DEBUG LINES:
                    // MinimaLogger::log("DEBUG_IBD: Greeting chain size: " + std::to_string(greetblocks.size()));
                    // MinimaLogger::log("DEBUG_IBD: My chain blocks size: " + std::to_string(myblocks.size()));
                    
                    // if (!greetblocks.empty()) {
                    //     MinimaLogger::log("DEBUG_IBD: First greeting block: " + greetblocks.front());
                    //     MinimaLogger::log("DEBUG_IBD: Last greeting block: " + greetblocks.back());
                    // }
                    
                    // if (!myblocks.empty()) {
                    //     // Print first few of our blocks
                    //     int count = 0;
                    //     for (const auto& blk : myblocks) {
                    //         if (count < 5) {
                    //             // MinimaLogger::log("DEBUG_IBD: My block[" + std::to_string(count) + "]: " + blk);
                    //             count++;
                    //         }
                    //     }
                    // }
                    // DEBUG END

                    isvalid = false;
                    createCompleteIBD();
                }
            }
        }

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }

    // Unlock
    org::minima::database::MinimaDB::getDB()->readLock(false);

    return isvalid;
}

void IBD::createCompleteIBD() {
    using org::minima::database::MinimaDB;

    // Copy current Cascade
    {
        //
        // FIX 1: Get 'casc' as a reference (auto&)
        //
        auto& casc = MinimaDB::getDB()->getCascade();
        
        //
        // FIX 2: Remove 'if (casc)' check and use dot '.' operator
        //
        auto cp = casc.deepCopy();
        mCascade = std::move(cp);
    }

    // Add all the blocks.. root will be first
    mTxBlocks.clear();
    auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
    while (tip != nullptr) {
        mTxBlocks.insert(mTxBlocks.begin(), std::shared_ptr<TxBlock>(tip, &(tip->getTxBlock())));
        tip = tip->getParent();
    }
}

void IBD::createSyncIBD(const org::minima::objects::TxPoW& zLastBlock) {
    using org::minima::database::MinimaDB;

    // No cascade
    mCascade.reset();

    //
    // FIX 3: Get 'arch' as a reference (auto&)
    //
    auto& arch = MinimaDB::getDB()->getArchive();

    MinimaDB::getDB()->readLock(true);

    try {
        if (org::minima::system::Main::getInstance()->isShuttingDown()) {
            MinimaDB::getDB()->readLock(false);
            return;
        }

        //
        // FIX 4: Use dot '.' operator on 'arch' reference
        //
        auto blocks = arch.loadSyncBlockRange(zLastBlock.getBlockNumber());
        for (auto& blk : blocks) {
            mTxBlocks.emplace_back(std::shared_ptr<TxBlock>(std::move(blk)));
        }

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }

    MinimaDB::getDB()->readLock(false);
}

void IBD::createArchiveIBD(const org::minima::objects::base::MiniNumber& zFirstBlock) {
    //
    // FIX 5: Get 'arch' as a reference (auto&)
    //
    auto& arch = org::minima::database::MinimaDB::getDB()->getArchive();
    
    //
    // FIX 6: Pass 'arch' by reference (remove '*')
    //
    createArchiveIBD(zFirstBlock, arch, false);
}

void IBD::createArchiveIBD(const org::minima::objects::base::MiniNumber& zFirstBlock,
                           org::minima::database::archive::ArchiveManager& zArchiveDB,
                           bool zUseLocal) {
    using org::minima::database::MinimaDB;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::MinimaLogger;

    if (org::minima::system::Main::getInstance()->isShuttingDown()) {
        return;
    }

    auto& arch = zArchiveDB; // Already a reference

    MinimaDB::getDB()->readLock(true);

    try {
        if (org::minima::system::Main::getInstance()->isShuttingDown()) {
            MinimaDB::getDB()->readLock(false);
            return;
        }

        MiniNumber startcount = zFirstBlock;

        if (zFirstBlock.isEqual(MiniNumber::ZERO())) {
            auto last = arch.loadLastBlock(); // Use dot '.'
            if (last) {
                if (!last->getTxPoW().getBlockNumber().isEqual(MiniNumber::ONE())) {
                    auto casc = arch.loadCascade(); // Use dot '.'
                    if (casc) {
                        startcount = casc->getTip()->getTxPoW().getBlockNumber().increment();
                        mCascade = std::move(casc);
                    }
                }
            }
        }

        // Load the block range
        MiniNumber end = startcount.add(MiniNumber::TWOFIVESIX());
        auto blocks = arch.loadBlockRange(startcount.decrement(), end, false); // Use dot '.'
        for (auto& blk : blocks) {
            mTxBlocks.emplace_back(std::shared_ptr<TxBlock>(std::move(blk)));
        }

        if (!zUseLocal) {
            auto root = MinimaDB::getDB()->getTxPoWTree().getRoot();
            MiniNumber rootblock = root->getTxPoW().getBlockNumber();

            MiniNumber reqblock = zFirstBlock;
            if (rootblock.isEqual(MiniNumber::ONE()) && reqblock.isEqual(MiniNumber::ZERO())) {
                MinimaLogger::log("Root Tree Archive IBD - starts at genesis");
                reqblock = MiniNumber::ONE();
            }

            if (rootblock.isEqual(reqblock)) {
                MinimaLogger::log("Archive request for main tree data @ " + reqblock.toString());

                // Add the whole tree (root first)
                std::shared_ptr<TxBlock> lastblock = nullptr;
                std::vector<std::shared_ptr<TxBlock>> mainblocks;
                auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
                while (tip != nullptr) {
                    lastblock = std::shared_ptr<TxBlock>(tip, &(tip->getTxBlock()));
                    mainblocks.insert(mainblocks.begin(), lastblock);
                    tip = tip->getParent();
                }

                // Check not changed
                org::minima::objects::base::MiniNumber lastadded = lastblock->getTxPoW().getBlockNumber();
                if (lastadded.isEqual(reqblock)) {
                    for ([[maybe_unused]] auto& blk : mainblocks) {
                        lastblock = std::shared_ptr<TxBlock>(tip, &(tip->getTxBlock()));
                    }
                } else {
                    MinimaLogger::log("Archive main tree data error.. root:" + lastadded.toString() +
                                      " req:" + reqblock.toString());
                }
            }
        }

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }

    MinimaDB::getDB()->readLock(false);
}

// Cascade
void IBD::setCascade(const org::minima::database::cascade::Cascade& zCascade) {
    // copy semantics
    mCascade = zCascade.deepCopy();
}

void IBD::setCascade(std::unique_ptr<org::minima::database::cascade::Cascade> zCascade) {
    mCascade = std::move(zCascade);
}

org::minima::database::cascade::Cascade* IBD::getCascade() const {
    return mCascade.get();
}

bool IBD::hasCascade() const {
    return static_cast<bool>(mCascade);
}

bool IBD::hasCascadeWithBlocks() const {
    return hasCascade() && (mCascade->getLength() > 0);
}

// Blocks
std::vector<std::shared_ptr<org::minima::objects::TxBlock>>& IBD::getTxBlocks() {
    return mTxBlocks;
}

const std::vector<std::shared_ptr<org::minima::objects::TxBlock>>& IBD::getTxBlocks() const {
    return mTxBlocks;
}

void IBD::setTxBlocks(std::vector<std::shared_ptr<org::minima::objects::TxBlock>> zBlocks) {
    mTxBlocks = std::move(zBlocks);
}

// Validation
bool IBD::checkValidData() const {
    using org::minima::objects::base::MiniNumber;
    using org::minima::database::cascade::Cascade;

    if (hasCascadeWithBlocks()) {
        if (mTxBlocks.size() == 0) {
            org::minima::utils::MinimaLogger::log("[!] Received INVALID IBD no blocks.. with a cascade");
            return false;
        } else {
            MiniNumber casctip = mCascade->getTip()->getTxPoW().getBlockNumber();
            MiniNumber treestart = mTxBlocks[0]->getTxPoW().getBlockNumber();

            if (!treestart.isEqual(casctip.increment())) {
                org::minima::utils::MinimaLogger::log(
                    "[!] Received INVALID IBD with cascade tip:" + casctip.toString() +
                    " and tree start:" + treestart.toString());
                return false;
            }

            bool checkcascade = Cascade::checkCascadeCorrect(*mCascade);
            if (!checkcascade) {
                org::minima::utils::MinimaLogger::log("[!] INCONSISTENT Cascade received! length:" +
                                                      std::to_string(mCascade->getLength()));
                return false;
            }
        }
    }

    return true;
}

// Weight
boost::multiprecision::cpp_int IBD::getTotalWeight() const {
    using org::minima::objects::base::MiniNumber;

    // Total weight of the chain (sum block weights using MiniNumber for precision)
    MiniNumber totalChain = MiniNumber::ZERO();
    for (const auto& block : mTxBlocks) {
        // TxPoW::getWeight returns a decimal string
        const std::string wstr = block->getTxPoW().getWeight();
        MiniNumber w(wstr);
        totalChain = totalChain.add(w);
    }
    boost::multiprecision::cpp_int chainweight = totalChain.getAsBigInteger();

    // Cascade weight as integer (exact BigDecimal -> BigInteger truncation)
    boost::multiprecision::cpp_int cascweight = 0;
    if (mCascade) {
        cascweight = mCascade->getTotalWeight().getAsBigInteger();
    }

    return cascweight + chainweight;
}

// Streamable
void IBD::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniByte;
    using org::minima::objects::base::MiniNumber;

    if (hasCascade()) {
        MiniByte::WriteToStream(out, true);
        mCascade->writeDataStream(out);
    } else {
        MiniByte::WriteToStream(out, false);
    }

    int len = static_cast<int>(mTxBlocks.size());
    MiniNumber::WriteToStream(out, len);

    for (const auto& block : mTxBlocks) {
        block->writeDataStream(out);
    }
}

void IBD::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniByte;
    using org::minima::objects::base::MiniNumber;

    if (MiniByte::ReadFromStream(in).isTrue()) {
        mCascade = std::make_unique<org::minima::database::cascade::Cascade>();
        mCascade->readDataStream(in);
    } else {
        mCascade.reset();
    }

    mTxBlocks.clear();
    int len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len > 0) mTxBlocks.reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        auto blk = org::minima::objects::TxBlock::ReadFromStream(in);
        // Support both unique_ptr/shared_ptr return types by moving into our shared_ptr container
        mTxBlocks.emplace_back(std::move(blk));
    }
}

IBD IBD::ReadFromStream(std::istream& in) {
    IBD ibd;
    ibd.readDataStream(in);
    return ibd;
}

// Heaviness comparison
bool IBD::checkOurChainHeavier(const IBD& zIBD) {
    using org::minima::database::cascade::CascadeNode;
    using org::minima::utils::MinimaLogger;

    // Build our own complete IBD
    IBD current;
    current.createCompleteIBD();

    bool found = false;
    std::string foundblockID;

    // Their chain blocks reversed (newest to oldest)
    std::vector<std::string> greetblocks;
    greetblocks.reserve(zIBD.getTxBlocks().size());
    for (const auto& block : zIBD.getTxBlocks()) {
        greetblocks.insert(greetblocks.begin(), block->getTxPoW().getTxPoWID());
    }

    std::vector<std::string> greetcascblocks;
    if (zIBD.getCascade()) {
        auto tip = zIBD.getCascade()->getTip();
        while (tip != nullptr) {
            greetcascblocks.push_back(tip->getTxPoW().getTxPoWID());
            tip = zIBD.getCascade()->getParent(tip);
        }
    }

    std::unordered_set<std::string> myblocks;
    for (const auto& block : current.getTxBlocks()) {
        myblocks.insert(block->getTxPoW().getTxPoWID());
    }

    std::unordered_set<std::string> mycascblocks;
    if (current.getCascade()) {
        auto tip = current.getCascade()->getTip();
        while (tip != nullptr) {
            mycascblocks.insert(tip->getTxPoW().getTxPoWID());
            tip = current.getCascade()->getParent(tip);
        }
    }

    // Intersections in four passes as in Java
    for (const auto& block : greetblocks) {
        if (myblocks.find(block) != myblocks.end()) {
            found = true; foundblockID = block; break;
        }
    }

    if (!found) {
        for (const auto& block : greetblocks) {
            if (mycascblocks.find(block) != mycascblocks.end()) {
                found = true; foundblockID = block; break;
            }
        }
    }

    if (!found) {
        for (const auto& block : greetcascblocks) {
            if (myblocks.find(block) != myblocks.end()) {
                found = true; foundblockID = block; break;
            }
        }
    }

    if (!found) {
        for (const auto& block : greetcascblocks) {
            if (mycascblocks.find(block) != mycascblocks.end()) {
                found = true; foundblockID = block; break;
            }
        }
    }

    boost::multiprecision::cpp_int myweight;
    boost::multiprecision::cpp_int theirweight;

    if (found) {
        IBD mynew = IBD::createShortenedIBD(current, foundblockID);
        myweight = mynew.getTotalWeight();

        IBD theirnew = IBD::createShortenedIBD(zIBD, foundblockID);
        theirweight = theirnew.getTotalWeight();
    } else {
        myweight = current.getTotalWeight();
        theirweight = zIBD.getTotalWeight();
    }

    bool heavier = (myweight >= theirweight);

    if (!heavier || GeneralParams::IBDSYNC_LOGS) {
        if (found) {
            IBD mynew = IBD::createShortenedIBD(current, foundblockID);
            IBD theirnew = IBD::createShortenedIBD(zIBD, foundblockID);
            MinimaLogger::log(std::string("[!] Crossover found on heavier chain : ") + foundblockID
                              + " myBlocks=" + std::to_string(mynew.getTxBlocks().size())
                              + " theirBlocks=" + std::to_string(theirnew.getTxBlocks().size())
                              + " myCascLen=" + std::to_string(mynew.getCascade() ? mynew.getCascade()->getLength() : 0)
                              + " theirCascLen=" + std::to_string(theirnew.getCascade() ? theirnew.getCascade()->getLength() : 0));
        } else {
            MinimaLogger::log("[!] NO crossover found on heavier chain..");
        }
        MinimaLogger::log(std::string("YOUR  WEIGHT from Crossover : ") + myweight.convert_to<std::string>());
        MinimaLogger::log(std::string("THEIR WEIGHT from Crossover : ") + theirweight.convert_to<std::string>());
    }

    return heavier;
}

IBD IBD::createShortenedIBD(const IBD& zIBD, const std::string& zTxPOWID) {
    IBD ibd;
    ibd.mCascade = std::make_unique<org::minima::database::cascade::Cascade>();

    bool found = false;
    const auto& blocks = zIBD.getTxBlocks();

    // Reverse copy of blocks
    std::vector<std::shared_ptr<TxBlock>> revblocks;
    revblocks.reserve(blocks.size());
    for (const auto& blk : blocks) {
        revblocks.insert(revblocks.begin(), blk);
    }

    for (const auto& block : revblocks) {
        ibd.mTxBlocks.insert(ibd.mTxBlocks.begin(), block);

        if (block->getTxPoW().getTxPoWID() == zTxPOWID) {
            found = true;
            break;
        }
    }

    if (!found) {
        // Collect cascade blocks as pointers to avoid copying TxPoW
        std::vector<const org::minima::objects::TxPoW*> txps;
        if (zIBD.getCascade()) {
            auto tip = zIBD.getCascade()->getTip();
            while (tip != nullptr) {
                txps.insert(txps.begin(), &tip->getTxPoW());
                if (tip->getTxPoW().getTxPoWID() == zTxPOWID) {
                    break;
                }
                tip = zIBD.getCascade()->getParent(tip);
            }
        }

        for (const auto* txp : txps) {
            ibd.mCascade->addToTip(*txp);
        }
        ibd.mCascade->cascadeChain();
    }

    return ibd;
}

void IBD::printIBD(const IBD& zIBD) {
    std::cout << "Cascade:" << std::endl;
    if (zIBD.getCascade()) {
        std::cout << zIBD.getCascade()->printCascade() << std::endl;
    } else {
        std::cout << "(none)" << std::endl;
    }

    std::cout << "Blocks:" << std::endl;
    for (const auto& block : zIBD.getTxBlocks()) {
        const org::minima::objects::TxPoW& txp = block->getTxPoW();
        std::cout << txp.getBlockNumber().toString() << ") " << txp.getTxPoWID() << std::endl;
    }

    std::cout << "Total Weight : " << zIBD.getTotalWeight().convert_to<std::string>() << std::endl;
    std::cout << std::endl;
}

} // namespace objects
} // namespace minima
} // namespace org