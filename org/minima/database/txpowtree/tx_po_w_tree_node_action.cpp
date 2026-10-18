#include "org/minima/database/txpowtree/tx_po_w_tree_node_action.hpp"

// Include full dependency definitions in source to break cycles
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

namespace org {
namespace minima {
namespace database {
namespace txpowtree {

TxPoWTreeNodeAction::TxPoWTreeNodeAction()
    : mReturnNode(nullptr), mExtraData() {
}

TxPoWTreeNodeAction::TxPoWTreeNodeAction(const std::string& zExtraData)
    : mReturnNode(nullptr), mExtraData(zExtraData) {
}

TxPoWTreeNodeAction::~TxPoWTreeNodeAction() = default;

bool TxPoWTreeNodeAction::isFinished() const {
    return mReturnNode != nullptr;
}

TxPoWTreeNode* TxPoWTreeNodeAction::getReturnNode() const {
    return mReturnNode;
}

void TxPoWTreeNodeAction::setReturnObject(TxPoWTreeNode* zNode) {
    mReturnNode = zNode;
}

const std::string& TxPoWTreeNodeAction::getExtraData() const {
    return mExtraData;
}

} // namespace txpowtree
} // namespace database
} // namespace minima
} // namespace org