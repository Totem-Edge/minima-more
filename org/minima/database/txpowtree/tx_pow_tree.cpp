#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <unordered_set>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/archive/tx_block_d_b.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/stack.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/utils/messages/message.hpp"

// Full header for TxPoWTreeNode (Rule 7/10)
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

namespace org {
namespace minima {
namespace database {
namespace txpowtree {

using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::archive::TxBlockDB;
using org::minima::utils::MiniFile;
using org::minima::utils::MinimaLogger;
using org::minima::utils::Stack;

namespace {


// Format milliseconds since epoch into a string akin to Java Date.toString()
// Cross-platform using std::put_time
inline std::string format_time_millis(long long millis) {
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::time_point(milliseconds(millis));
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmv, "%a %b %d %H:%M:%S %Y");
    return oss.str();
}

} // anonymous namespace

TxPowTree::TxPowTree()
    : mRoot(nullptr)
    , mTip(nullptr)
    , mLength(0)
    , mTotalNodes(0)
    , mFastLink()
    , mPulseList() {
}

std::shared_ptr<TxPoWTreeNode> TxPowTree::findNode(const std::string& zTxPoWID) {
    auto it = mFastLink.find(zTxPoWID);
    if (it == mFastLink.end()) {
        return nullptr;
    }
    return it->second;
}

void TxPowTree::setRoot(const std::shared_ptr<TxPoWTreeNode>& zTreeNode) {
    
    mRoot = zTreeNode;
    if (mRoot) {
        mRoot->clearParent();
    }
    
    // This function will set mTip and populate mFastLink
    recalculateTree(); 
}

std::shared_ptr<TxPoWTreeNode> TxPowTree::getRoot() const {
    return mRoot;
}

std::shared_ptr<TxPoWTreeNode> TxPowTree::getTip() {
    return mTip;
}

int TxPowTree::getSize() const {
    return static_cast<int>(mFastLink.size());
}

void TxPowTree::addFastLink(const std::shared_ptr<TxPoWTreeNode>& zNode) {
    if (!zNode) return;
    auto& txp = zNode->getTxPoW();
    
    mFastLink[txp.getTxPoWID()] = zNode;
    
}

void TxPowTree::recalculateTree() {
    // New Fast Link..
    mFastLink.clear();

    // Create a complete list of all nodes in the tree
    std::vector<std::shared_ptr<TxPoWTreeNode>> allnodes;
    allnodes.reserve(1024);

    // First set all the weights to their base weight and fill fast link
    class ResetAction final : public TxPoWTreeNodeAction {
    public:
        ResetAction(std::vector<std::shared_ptr<TxPoWTreeNode>>& allnodes,
                    std::unordered_map<std::string, std::shared_ptr<TxPoWTreeNode>>& fastlink)
            : m_allnodes(allnodes), m_fastlink(fastlink) {}

        void runAction(const std::shared_ptr<TxPoWTreeNode>& zNode) override {
            if (!zNode) return;
            auto& txp = zNode->getTxPoW();
            
            auto weight = txp.getWeight(); // BigDecimal -> std::string (per mapping)
            zNode->setTotalWeight(MiniNumber::fromWideString(weight));
            m_allnodes.push_back(zNode);
            m_fastlink[txp.getTxPoWID()] = zNode;
            
        }
    private:
        std::vector<std::shared_ptr<TxPoWTreeNode>>& m_allnodes;
        std::unordered_map<std::string, std::shared_ptr<TxPoWTreeNode>>& m_fastlink;
    } reset(allnodes, mFastLink);

    traverseTree(reset);

    // Reset total nodes
    mTotalNodes = static_cast<int>(mFastLink.size());

    // Now organise all nodes into descending order of block number
    std::sort(allnodes.begin(), allnodes.end(),
              [](const std::shared_ptr<TxPoWTreeNode>& a, const std::shared_ptr<TxPoWTreeNode>& b) {
                  if (!a || !b) return false;
                  auto& atxp = a->getTxPoW();
                  auto& btxp = b->getTxPoW();
                //   if (!atxp || !btxp) return false;
                  // return b.blockNumber > a.blockNumber
                  return btxp.getBlockNumber().compareTo(atxp.getBlockNumber()) > 0;
              });

    // Now cycle through and add the weight of the children to the parents
    for (auto& node : allnodes) {
        if (!node) continue;
        auto children = node->getChildren();
        for (auto& child : children) {
            if (!child) continue;
            node->addToTotalWeight(child->getTotalWeight());
        }
    }

    // And find the heaviest branch tip..
    mTip = getRoot();
    mLength = 0;

    while (mTip != nullptr) {
        // Increase length
        mLength++;

        auto children = mTip->getChildren();
        if (children.empty()) {
            break;
        }

        // Only keep the heaviest
        bool firstchild = true;
        std::shared_ptr<TxPoWTreeNode> best = mTip;
        for (auto& child : children) {
            if (!child) continue;
            if (firstchild) {
                firstchild = false;
                best = child;
            } else {
                // Compare child's total weight vs current best total weight
                // Use MiniNumber::compareTo() to match Java BigDecimal.compareTo() (not double >)
                const auto& ctot = child->getTotalWeight();
                const auto& btot = best->getTotalWeight();

                if (ctot.compareTo(btot) > 0) {
                    best = child;
                }
            }
        }
        if (best == mTip) {
            // No better child found
            break;
        }
        mTip = best;
    }

    // Do this once.. used every time you receive a pulse
    calculatePulseList();
}

const std::vector<MiniData>& TxPowTree::getPulseList() const {
    return mPulseList;
}

void TxPowTree::calculatePulseList() {
    std::vector<MiniData> blocklist;
    blocklist.reserve(512);

    auto current = getTip();
    int counter = 0;
    while (current != nullptr && counter < 512) {
        auto& txp = current->getTxPoW();
        
        blocklist.push_back(txp.getTxPoWIDData());
        
        current = current->getParent();
        counter++;
    }

    // And switch..
    mPulseList = std::move(blocklist);
}

std::vector<std::shared_ptr<TxPoWTreeNode>> TxPowTree::getHeaviestBranch() {
    // Cycle back from the tip
    std::vector<std::shared_ptr<TxPoWTreeNode>> hbranch;
    auto current = getTip();
    while (current != nullptr) {
        hbranch.push_back(current);
        current = current->getParent();
    }
    return hbranch;
}

int TxPowTree::getHeaviestBranchLength() const {
    return mLength;
}

std::vector<std::shared_ptr<TxPoWTreeNode>> TxPowTree::setLength(int zMaxLength) {
    // The section of all blocks that are removed (main chain + side branches)
    std::vector<std::shared_ptr<TxPoWTreeNode>> removed;

    int counter = 0;
    std::shared_ptr<TxPoWTreeNode> newroot = nullptr;
    auto current = getTip();
    
    // Find the new root by walking back from tip
    while (current != nullptr) {
        newroot = current;
        counter++;
        if (counter >= zMaxLength) {
            break;
        }
        current = current->getParent();
    }

    // If no newroot found, clear everything
    if (!newroot) {
        setRoot(nullptr);
        return removed;
    }

    // Get the parent of the new root - this is where cascade starts
    auto cascade = newroot->getParent();

    // Walk backward through parent chain (like Java)
    // This ensures blocks are in order: oldest → newest
    while (cascade != nullptr) {
        // Add to FRONT of vector (equivalent to Java's add(0, x))
        removed.insert(removed.begin(), cascade);
        
        // Move to parent (going backward in time)
        cascade = cascade->getParent();
    }

    // Get the new root's block number for side branch collection
    MiniNumber newRootBlockNum = newroot->getBlockNumber();

    // Now collect any side branches (blocks not on main chain)
    // These are blocks with block number < newroot that aren't in removed already
    std::unordered_set<std::string> mainChainIDs;
    for (const auto& node : removed) {
        mainChainIDs.insert(node->getTxPoW().getTxPoWID());
    }

    for (const auto& pair : mFastLink) {
        const std::shared_ptr<TxPoWTreeNode>& node = pair.second;
        if (node->getBlockNumber().isLess(newRootBlockNum)) {
            // Only add if not already in main chain
            if (mainChainIDs.find(node->getTxPoW().getTxPoWID()) == mainChainIDs.end()) {
                removed.push_back(node);  // Side branches go at the end
            }
        }
    }

    // Set the new root (will prune everything below and recalculate)
    setRoot(newroot);

    // Return ALL removed blocks (so they can be added to cascade)
    return removed;
}

std::string TxPowTree::printTree(int zDepth) {
    std::ostringstream treestr;

    // What is the root block..
    auto tip = getTip();
    if (tip == nullptr) {
        return "";
    }

    // Cycle back..
    for (int i = 0; i < zDepth - 1; i++) {
        if (tip->getParent() != nullptr) {
            tip = tip->getParent();
        }
    }

    auto& txp_tip = tip->getTxPoW();
    
    
    
    MiniNumber rootblock = txp_tip.getBlockNumber();

    // Printer action
    class PrinterAction final : public TxPoWTreeNodeAction {
    public:
        PrinterAction(std::ostringstream& out, const MiniNumber& rootblock)
            : m_out(out), m_rootblock(rootblock) {}

        void runAction(const std::shared_ptr<TxPoWTreeNode>& zNode) override {
            if (!zNode) return;
            auto& txp = zNode->getTxPoW();
            

            auto weight = txp.getWeight();
            std::string ID = txp.getTxPoWID();
            MiniNumber block = txp.getBlockNumber();
            int sblk = txp.getSuperLevel();

            // How much to indent..
            MiniNumber indent = block.sub(m_rootblock);
            int ind = indent.getAsInt();
            for (int i = 0; i < ind; i++) {
                if (i == ind - 1) {
                    m_out << "-->";
                } else {
                    m_out << "   ";
                }
            }

            // Number of Transactions
            int numtxns = static_cast<int>(txp.getTransactions().size());
            if (txp.isTransaction()) {
                numtxns++;
            }

            long long millis = txp.getTimeMilli().getAsLong();
            m_out << " " << block.toString()
                  << " [0/" << sblk << "] "
                  << ID
                  << " txns:" << numtxns
                  << "  weight:" << weight << "/" << zNode->getTotalWeight().toString()
                  << " @ " << format_time_millis(millis)
                  << "\n";
        }

    private:
        std::ostringstream& m_out;
        MiniNumber m_rootblock;
    } printer(treestr, rootblock);

    // Traverse the tree from the chosen tip (as root anchor)
    traverseTree(printer, tip);

    // And return the final string
    return treestr.str();
}

std::shared_ptr<TxPoWTreeNode> TxPowTree::traverseTree(TxPoWTreeNodeAction& zNodeAction) {
    return traverseTree(zNodeAction, getRoot());
}

std::shared_ptr<TxPoWTreeNode> TxPowTree::traverseTree(TxPoWTreeNodeAction& zNodeAction,
                                                       const std::shared_ptr<TxPoWTreeNode>& zRoot) {
    Stack stack;

    // If nothing on chain return nothing
    if (!zRoot) {
        return nullptr;
    }

    // Push the root on the stack
    stack.push(zRoot);

    // Now cycle..
    while (!stack.isEmpty()) {
        // Get the top stack item
        std::any top = stack.pop();
        if (!top.has_value()) {
            continue;
        }

        std::shared_ptr<TxPoWTreeNode> node;
        try {
            node = std::any_cast<std::shared_ptr<TxPoWTreeNode>>(top);
        } catch (const std::bad_any_cast&) {
            continue;
        }
        if (!node) {
            continue;
        }

        // Do the action..
        zNodeAction.runAction(node);

        // Have we found what we were looking for..
        if (zNodeAction.isFinished()) {
            return zNodeAction.getReturnNode();
        }

        // Get any children..
        auto children = node->getChildren();
        for (auto& child : children) {
            // Push on the stack
            stack.push(child);
        }
    }

    return nullptr;
}

void TxPowTree::loadDB(const std::filesystem::path& zFile) {
    MiniFile::loadObjectSlow(zFile, *this);
}

void TxPowTree::saveDB(const std::filesystem::path& zFile) {
    MiniFile::saveObjectDirect(zFile, *this);
}

void TxPowTree::writeDataStream(std::ostream& out) {
    // How many nodes..
    MiniNumber::WriteToStream(out, mTotalNodes);

    // Cycle through and output the whole tree
    class WriteOutAction final : public TxPoWTreeNodeAction {
    public:
        explicit WriteOutAction(std::ostream& out) : m_out(out) {}

        void runAction(const std::shared_ptr<TxPoWTreeNode>& zNode) override {
            if (!zNode) return;
            try {
                zNode->writeDataStream(m_out);
            } catch (const std::exception& e) {
                MinimaLogger::log(e);
            }
        }
    private:
        std::ostream& m_out;
    } writeout(out);

    // Traverse the tree
    traverseTree(writeout);
}

void TxPowTree::readDataStream(std::istream& in) {
    // Reset params
    mRoot = nullptr;
    mTip = nullptr;
    mFastLink.clear();
    mLength = 0;

    // Get the RequiredDBs
    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
    TxBlockDB& txbdb = MinimaDB::getDB()->getTxBlockDB();

    int len = MiniNumber::ReadFromStream(in).getAsInt();
    MinimaLogger::log("TxPowTree: Loading " + std::to_string(len) + " nodes from database");
    
    int successful = 0;
    int failed = 0;
    
    for (int i = 0; i < len; ++i) {
        try {
            // Read in a node..
            std::shared_ptr<TxPoWTreeNode> node = TxPoWTreeNode::ReadFromStream(in);

            // VALIDATION: Check if node is valid
            if (!node) {
                MinimaLogger::log("ERROR TxPowTree: Node " + std::to_string(i) + " is null, skipping");
                failed++;
                continue;
            }

            auto& txp = node->getTxPoW();
            
            // VALIDATION: Check if TxPoWID is valid (this indirectly validates TxHeader)
            if (txp.getTxPoWID().empty()) {
                MinimaLogger::log("ERROR TxPowTree: Node " + std::to_string(i) + 
                                " has empty TxPoWID (TxHeader may be invalid), skipping");
                failed++;
                continue;
            }
            
            // VALIDATION: Check if block number is valid
            try {
                auto blockNum = txp.getBlockNumber();
                // If we get here, TxHeader is valid
            } catch (const std::exception& e) {
                MinimaLogger::log("ERROR TxPowTree: Node " + std::to_string(i) + 
                                " TxHeader access failed: " + std::string(e.what()) + ", skipping");
                failed++;
                continue;
            }
            
            // VALIDATION: Check if weight is valid
            std::string weight = txp.getWeight();
            if (weight.empty()) {
                MinimaLogger::log("WARNING TxPowTree: Node " + std::to_string(i) + 
                                ", TxPoWID=" + txp.getTxPoWID() + 
                                " has empty weight, defaulting to 0");
                // Note: This should be fixed in TxPoW itself, but we can continue
            }

            // Log successful node load
            // MinimaLogger::log("DEBUG TxPowTree: Node " + std::to_string(i) + 
            //                 " TxPoWID=" + txp.getTxPoWID() + 
            //                 ", Block=" + txp.getBlockNumber().toString() +
            //                 ", Weight='" + weight + "'");

            // Add to the fast link table..
            addFastLink(node);

            // Add the TxPoW to the main TxPoWDB - just SQL not Mempool..
            const TxPoW& txpref = node->getTxPoW();
            auto txp_copy_uniq = txpref.deepCopy();
            auto txp_shared = std::shared_ptr<TxPoW>(std::move(txp_copy_uniq));
            txpdb.addSQLTxPoW(txp_shared);

            // Add TxBlock to database
            auto txb = std::make_shared<org::minima::objects::TxBlock>(node->getTxBlock());
            txbdb.addTxBlock(txb);

            // Add it to the tree..
            if (!mRoot) {
                mRoot = node;
                if (mRoot) {
                    mRoot->clearParent();
                    mTip = node;
                }
            } else {
                // Find the parent
                const TxPoW& txp_ref = node->getTxPoW();
                
                std::string parentid = txpref.getParentID().to0xString();
                std::shared_ptr<TxPoWTreeNode> parent = findNode(parentid);

                // Add to the parent if found
                if (parent) {
                    parent->addChildNode(node);
                } else {
                    // Parent not found - this can happen after blockchain reorganization
                    // The node references a block from an old fork that's no longer in the cascade
                    
                    // Check if this is a root-level orphan that should be skipped
                    if (successful == 0) {
                        // This is the first node and has no parent - might be ok
                        MinimaLogger::log("INFO TxPowTree: First node " + std::to_string(i) + 
                                        " has no parent - may be cascade misalignment");
                    } else {
                        // This is an orphaned node from an old fork
                        MinimaLogger::log("INFO TxPowTree: Skipping orphaned node " + 
                                        std::to_string(i) + 
                                        " TxPoWID=" + txp_ref.getTxPoWID() + 
                                        " (parent not found: " + parentid + 
                                        ") - likely from old fork after reorganization");
                        
                        // Don't increment successful count
                        continue;
                    }
                }
            }
            
            successful++;
            
        } catch (const std::exception& e) {
            failed++;
            MinimaLogger::log("ERROR TxPowTree: Failed to load node " + std::to_string(i) + 
                            ": " + std::string(e.what()));
            // A partial tree is unsafe: abort the whole load and let startup
            // quarantine/resync instead of running with missing/corrupt nodes.
            throw;
        }
    }

    // MinimaLogger::log("TxPowTree: Loaded " + std::to_string(successful) + " nodes successfully, " + 
    //                  std::to_string(failed) + " failed");

    // Recalculate the tree
    if (mRoot) {
        recalculateTree();
    } else {
        MinimaLogger::log("WARNING TxPowTree: No valid root node after loading, tree is empty");
    }
}


} // namespace txpowtree
} // namespace database
} // namespace minima
} // namespace org