#include "org/minima/database/cascade/cascade.hpp"

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <set>
#include <shared_mutex>

#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace database {
namespace cascade {

using org::minima::utils::MinimaLogger;

Cascade::Cascade() {
    mNodes.reserve(10000);
}

Cascade::~Cascade() = default;

void Cascade::addToTip(const org::minima::objects::TxPoW& zTxPoW) {
    std::unique_lock lock(mMutex);

    auto node = std::make_shared<CascadeNode>(zTxPoW);

    if (mTip != nullptr) {
        node->setParent(mTip.get());
    }

    mNodes.push_back(node);
    mTip = node;
}

std::shared_ptr<CascadeNode> Cascade::getTip() const {
    std::shared_lock lock(mMutex);
    return mTip;
}

org::minima::objects::base::MiniNumber Cascade::getTotalWeight() const {
    std::shared_lock lock(mMutex);
    return mTotalWeight;
}

int Cascade::getLength() const {
    std::shared_lock lock(mMutex);
    int length = 0;
    std::shared_ptr<CascadeNode> current = mTip;
    while (current != nullptr) {
        ++length;
        current = getParent(current);
    }
    return length;
}

void Cascade::cascadeChain() {
    std::unique_lock lock(mMutex);

    if (!mTip) return;

    std::shared_ptr<CascadeNode> newcascade = mTip;
    std::shared_ptr<CascadeNode> current = getParentNoLock(mTip);

    org::minima::objects::base::MiniNumber totalWeightCalc = org::minima::objects::base::MiniNumber::fromWideString(newcascade->getCurrentWeight());

    int casclevel = 0;
    int totlevel  = 1;

    std::set<CascadeNode*> keptNodes;
    keptNodes.insert(newcascade.get());

    while (current != nullptr) {
        int superlev = current->getSuperLevel();

        if (superlev >= casclevel) {
            current->setLevel(casclevel);
            newcascade->setParent(current.get());
            newcascade = current;
            keptNodes.insert(current.get());

            totalWeightCalc = totalWeightCalc.add(
                org::minima::objects::base::MiniNumber::fromWideString(newcascade->getCurrentWeight()));

            totlevel++;
            if (totlevel >= org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVEL_NODES) {
                if (casclevel < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS - 1) {
                    casclevel++;
                    totlevel = 0;
                }
            }
        }

        current = getParentNoLock(current);
    }

    if (newcascade) {
        newcascade->setParent(nullptr);
    }

    mTotalWeight = totalWeightCalc;

    std::vector<std::shared_ptr<CascadeNode>> validNodes;
    validNodes.reserve(keptNodes.size());

    for (auto& nodePtr : mNodes) {
        if (keptNodes.count(nodePtr.get()) > 0) {
            validNodes.push_back(nodePtr);
        }
    }
    mNodes = std::move(validNodes);
}

std::unique_ptr<Cascade> Cascade::deepCopy() const {
    std::shared_lock lock(mMutex);
    std::ostringstream baos(std::ios::binary);
    const_cast<Cascade*>(this)->writeDataStream(baos);
    const std::string bytes = baos.str();

    std::istringstream bais(bytes, std::ios::binary);
    auto deepcopy = std::make_unique<Cascade>();
    deepcopy->readDataStream(bais);
    return deepcopy;
}

std::string Cascade::printCascade() const {
    std::shared_lock lock(mMutex);
    std::string casstr;

    std::vector<std::shared_ptr<CascadeNode>> nodes;
    std::shared_ptr<CascadeNode> current = mTip;
    while (current != nullptr) {
        nodes.insert(nodes.begin(), current);
        current = getParent(current);
    }

    for (const auto& node : nodes) {
        casstr += node->toString();
        casstr += "\n";
    }

    return casstr;
}

void Cascade::loadDB(const std::filesystem::path& zFile) {
    org::minima::utils::MiniFile::loadObjectSlow(zFile, *this);
}

void Cascade::saveDB(const std::filesystem::path& zFile) {
    std::shared_lock lock(mMutex);
    org::minima::utils::MiniFile::saveObjectDirect(zFile, *this);
}

void Cascade::writeDataStream(std::ostream& out) {
    std::shared_lock lock(mMutex);

    int len = 0;
    std::shared_ptr<CascadeNode> current = mTip;
    while (current != nullptr) {
        ++len;
        current = getParent(current);
    }

    org::minima::objects::base::MiniNumber::WriteToStream(out, len);

    current = mTip;
    while (current != nullptr) {
        current->writeDataStream(out);
        current = getParent(current);
    }
}

void Cascade::readDataStream(std::istream& in) {
    std::unique_lock lock(mMutex);

    int len = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();

    MinimaLogger::log("Cascade: Loading " + std::to_string(len) + " nodes from database");

    mNodes.clear();
    mTip = nullptr;
    mTotalWeight = org::minima::objects::base::MiniNumber::ZERO();

    mNodes.reserve(std::max(len, 10000));

    std::shared_ptr<CascadeNode> current = nullptr;
    int successful = 0;
    int failed = 0;

    for (int i = 0; i < len; i++) {
        try {
            auto node = CascadeNode::ReadFromStream(in);

            if (!node) {
                MinimaLogger::log("ERROR Cascade: Node " + std::to_string(i) + " returned null, skipping");
                failed++;
                continue;
            }

            auto& txpow = node->getTxPoW();
            if (txpow.getTxPoWID().empty()) {
                MinimaLogger::log("ERROR Cascade: Node " + std::to_string(i) + " has empty TxPoWID, skipping");
                failed++;
                continue;
            }

            try {
                auto blockNum = txpow.getBlockNumber();
            } catch (const std::exception& e) {
                MinimaLogger::log("ERROR Cascade: Node " + std::to_string(i) + " TxHeader access failed: " + std::string(e.what()) + ", skipping");
                failed++;
                continue;
            }

            auto sharedNode = std::shared_ptr<CascadeNode>(std::move(node));
            mNodes.push_back(sharedNode);

            if (current != nullptr) {
                current->setParent(sharedNode.get());
            } else {
                mTip = sharedNode;
            }

            current = sharedNode;
            successful++;

        } catch (const std::exception& e) {
            failed++;
            MinimaLogger::log("ERROR Cascade: Failed to load node " + std::to_string(i) + ": " + std::string(e.what()));
            if (failed > 5) {
                MinimaLogger::log("ERROR Cascade: Too many consecutive failures (" + std::to_string(failed) + "), stopping cascade load");
                break;
            }
            continue;
        }
    }

    MinimaLogger::log("Cascade: Loaded " + std::to_string(successful) + " nodes successfully, " + std::to_string(failed) + " failed");

    if (mTip) {
        // cascadeChain() re-acquires mMutex; release our lock first to avoid self-deadlock
        lock.unlock();
        cascadeChain();
    } else {
        MinimaLogger::log("WARNING Cascade: No valid tip node after loading");
    }
}


std::unique_ptr<Cascade> Cascade::convertMiniDataVersion(const org::minima::objects::base::MiniData& zCascData) {
    const std::vector<std::uint8_t>& bytes = zCascData.getBytes();
    std::string data(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream dis(data, std::ios::binary);

    std::unique_ptr<Cascade> cascade;
    try {
        cascade = std::make_unique<Cascade>();
        cascade->readDataStream(dis);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return cascade;
}

std::unique_ptr<Cascade> Cascade::ReadFromStream(std::istream& in) {
    auto cascade = std::make_unique<Cascade>();
    cascade->readDataStream(in);
    return cascade;
}

bool Cascade::checkCascadeCorrect(const Cascade& zCascade) {
    std::shared_ptr<CascadeNode> cnode = zCascade.getTip();

    if (cnode != nullptr && cnode->getLevel() != 0) {
        MinimaLogger::log(std::string("Cascade does not start at level 0.. ") + std::to_string(cnode->getLevel()));
        return false;
    }

    int counter  = 0;
    int oldlevel = 0;
    while (cnode != nullptr) {
        org::minima::objects::TxPoW& txp = cnode->getTxPoW();
        int clevel = cnode->getLevel();

        if (clevel != oldlevel) {
            if (clevel != oldlevel + 1) {
                MinimaLogger::log("NEXT level up is not a single increment @ clevel:" + std::to_string(clevel) + " oldlevel:" + std::to_string(oldlevel));
                return false;
            }
            counter = 0;
            oldlevel = clevel;
        }

        CascadeNode* pnode = cnode->getParent();
        bool lastnode = (pnode == nullptr) || (pnode->getLevel() != clevel);

        bool foundzero = false;
        for (int i = clevel; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; ++i) {
            std::string sparent = txp.getSuperParent(i).to0xString();
            if (sparent == "0x00") {
                foundzero = true;
            } else {
                if (foundzero) {
                    MinimaLogger::log("NON zero node found in cascade after first zero node.." + sparent + " @ slevel " + std::to_string(i) + "/" + std::to_string(clevel) + " counter:" + std::to_string(counter));
                    return false;
                }
                if (lastnode) {
                    if (i > clevel) {
                        if (!checkPastNodeExists(cnode.get(), sparent)) {
                            MinimaLogger::log("Parent not found in cascade.. " + sparent + " @ slevel " + std::to_string(i) + "/" + std::to_string(clevel) + " counter:" + std::to_string(counter));
                            return false;
                        }
                    }
                    } else {
                    if (!checkPastNodeExists(cnode.get(), sparent)) {
                        MinimaLogger::log("Parent not found in cascade.. " + sparent + " @ slevel " + std::to_string(i) + "/" + std::to_string(clevel) + " counter:" + std::to_string(counter));
                        return false;
                    }
                }
            }
        }

        cnode = zCascade.getParent(cnode);
        counter++;
    }

    return true;
}

bool Cascade::checkPastNodeExists(CascadeNode* zCascadeNode, const std::string& zTxPoWID) {
    CascadeNode* cnode = zCascadeNode;
    while (cnode != nullptr) {
        if (cnode->getTxPoW().getTxPoWID() == zTxPoWID) {
            return true;
        }
        cnode = cnode->getParent();
    }
    return false;
}

bool Cascade::hasBlock(const std::string& zTxPoWID) const {
    std::shared_lock lock(mMutex);
    std::shared_ptr<CascadeNode> current = mTip;
    while (current != nullptr) {
        if (current->getTxPoW().getTxPoWID() == zTxPoWID) {
            return true;
        }
        current = getParent(current);
    }
    return false;
}

std::shared_ptr<CascadeNode> Cascade::getParentNoLock(const std::shared_ptr<CascadeNode>& zNode) const {
    if (!zNode) return nullptr;
    CascadeNode* rawParent = zNode->getParent();
    if (!rawParent) return nullptr;

    for (auto& n : mNodes) {
        if (n.get() == rawParent) {
            return n;
        }
    }
    return nullptr;
}

std::shared_ptr<CascadeNode> Cascade::getParent(const std::shared_ptr<CascadeNode>& zNode) const {
    if (!zNode) return nullptr;
    CascadeNode* rawParent = zNode->getParent();
    if (!rawParent) return nullptr;

    std::shared_lock lock(mMutex);
    for (auto& n : mNodes) {
        if (n.get() == rawParent) {
            return n;
        }
    }
    return nullptr;
}

} // namespace cascade
} // namespace database
} // namespace minima
} // namespace org
