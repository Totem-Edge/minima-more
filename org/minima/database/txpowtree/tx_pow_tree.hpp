#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org { namespace minima { namespace objects { class TxPoW; class TxBlock; } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace txpowdb { class TxPoWDB; } } } }
namespace org { namespace minima { namespace database { namespace archive { class TxBlockDB; } } } }
namespace org { namespace minima { namespace utils { class Stack; } } }

// Forward declaration for circular dependency (Rule 10)
namespace org { namespace minima { namespace database { namespace txpowtree {
class TxPoWTreeNode;
} } } }

namespace org {
namespace minima {
namespace database {
namespace txpowtree {

class TxPowTree : public org::minima::utils::Streamable {
public:
    // Action interface matching the Java anonymous classes used in traversals
    class TxPoWTreeNodeAction {
    public:
        virtual ~TxPoWTreeNodeAction() = default;
        virtual void runAction(const std::shared_ptr<TxPoWTreeNode>& zNode) = 0;
        virtual bool isFinished() const { return false; }
        virtual std::shared_ptr<TxPoWTreeNode> getReturnNode() const { return nullptr; }
    };

    TxPowTree();

    // Lookup by TxPoWID (hex string like "0x.."); returns nullptr if not found
    std::shared_ptr<TxPoWTreeNode> findNode(const std::string& zTxPoWID);

    void setRoot(const std::shared_ptr<TxPoWTreeNode>& zTreeNode);

    std::shared_ptr<TxPoWTreeNode> getRoot() const;
    std::shared_ptr<TxPoWTreeNode> getTip();

    int getSize() const;

    void addFastLink(const std::shared_ptr<TxPoWTreeNode>& zNode);

    void recalculateTree();

    const std::vector<org::minima::objects::base::MiniData>& getPulseList() const;

    std::vector<std::shared_ptr<TxPoWTreeNode>> getHeaviestBranch();
    int getHeaviestBranchLength() const;

    // Trim to a maximum length from the tip; returns the removed section from the previous longest branch
    std::vector<std::shared_ptr<TxPoWTreeNode>> setLength(int zMaxLength);

    std::string printTree(int zDepth);

    // Persistence
    void loadDB(const std::filesystem::path& zFile);
    void saveDB(const std::filesystem::path& zFile);

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    std::shared_ptr<TxPoWTreeNode> traverseTree(TxPoWTreeNodeAction& zNodeAction);
    std::shared_ptr<TxPoWTreeNode> traverseTree(TxPoWTreeNodeAction& zNodeAction,
                                                const std::shared_ptr<TxPoWTreeNode>& zRoot);

    void calculatePulseList();

private:
    // Root node of the whole tree
    std::shared_ptr<TxPoWTreeNode> mRoot;

    // Tip of the heaviest branch on the tree based on GHOST
    std::shared_ptr<TxPoWTreeNode> mTip;

    // The current length of the heaviest Branch
    int mLength = 0;

    // How many nodes in total
    int mTotalNodes = 0;

    // A hashtable o(1) look up table to find nodes in the tree
    std::unordered_map<std::string, std::shared_ptr<TxPoWTreeNode>> mFastLink;

    // The PULSE list.. a ready to use list of TxPoWID from tip..
    // Updated whenever we recalculate the tree
    std::vector<org::minima::objects::base::MiniData> mPulseList;
};

} // namespace txpowtree
} // namespace database
} // namespace minima
} // namespace org