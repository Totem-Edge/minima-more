#pragma once

#include <string>

namespace org {
namespace minima {
namespace database {
namespace txpowtree {

// Forward declaration for class in the same namespace
class TxPoWTreeNode;

class TxPoWTreeNodeAction {
public:
    TxPoWTreeNodeAction();
    explicit TxPoWTreeNodeAction(const std::string& zExtraData);
    virtual ~TxPoWTreeNodeAction();

    bool isFinished() const;
    
    // RULE 3 FIX: Returns namespaced pointer
    TxPoWTreeNode* getReturnNode() const;
    // RULE 3 FIX: Uses namespaced pointer
    void setReturnObject(TxPoWTreeNode* zNode);
    const std::string& getExtraData() const;

    // Main recurse function
    // RULE 3 FIX: Uses namespaced pointer
    virtual void runAction(TxPoWTreeNode* zNode) = 0;

private:
    // RULE 2 FIX: Member is a namespaced raw pointer (as it's non-owning)
    TxPoWTreeNode* mReturnNode;
    std::string    mExtraData;
};

} // namespace txpowtree
} // namespace database
} // namespace minima
} // namespace org
