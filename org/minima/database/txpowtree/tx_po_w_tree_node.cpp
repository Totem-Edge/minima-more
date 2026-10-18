#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

// Full includes for used types
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
// Include OnChain DB header to complete type for addOnChainTxPoW
#include "org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.hpp"

#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/state_variable.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

#include <utility>
#include <algorithm>
#include <memory>
#include <stdexcept>

using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::StateVariable;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntry;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONObject;
using org::minima::utils::MinimaLogger;

namespace {

// Convert std::vector<StateVariable> to JSON using Coin::convertStateListToJSON,
// by copying into a temporary vector<std::unique_ptr<StateVariable>>.
org::minima::utils::json::JSONObject ConvertStateVecToJSON(const std::vector<StateVariable>& vec) {
    std::vector<std::unique_ptr<StateVariable>> tmp;
    tmp.reserve(vec.size());
    for (const auto& sv : vec) {
        tmp.emplace_back(std::make_unique<StateVariable>(sv));
    }
    return Coin::convertStateListToJSON(tmp);
}

} // anonymous namespace

namespace org {
namespace minima {
namespace database {
namespace txpowtree {

std::unique_ptr<TxBlock> TxPoWTreeNode::deepCopyTxBlock(const TxBlock& zTxBlock) {
    // Serialize to MiniData and reconstruct
    // MiniData::getMiniDataVersion requires non-const Streamable&
    TxBlock& nonconst = const_cast<TxBlock&>(zTxBlock);
    std::unique_ptr<MiniData> md = MiniData::getMiniDataVersion(nonconst);
    if (!md) {
        throw std::runtime_error("Failed to serialize TxBlock for deep copy");
    }
    std::unique_ptr<TxBlock> copy = TxBlock::convertMiniDataVersion(*md);
    if (!copy) {
        throw std::runtime_error("Failed to deserialize TxBlock for deep copy");
    }
    return copy;
}

TxPoWTreeNode::TxPoWTreeNode()
    : mTxBlock(nullptr),
      mParent(),
      mChildren(),
      mTotalWeight(MiniNumber::ZERO()),
      mMMR(nullptr),
      mCoins(),
      mRelevantMMRCoins(),
      mComputedRelevantCoins(),
      mHaveCheckedFull(false) {}

TxPoWTreeNode::TxPoWTreeNode(const TxBlock& zTxBlock)
    : TxPoWTreeNode(zTxBlock, true) {}

TxPoWTreeNode::TxPoWTreeNode(const TxBlock& zTxBlock, bool zFindRelevant)
    : mTxBlock(std::make_unique<TxBlock>(zTxBlock)),
      mParent(),
      mChildren(),
      mTotalWeight(MiniNumber::ZERO()),
      mMMR(nullptr),
      mCoins(),
      mRelevantMMRCoins(),
      mComputedRelevantCoins(),
      mHaveCheckedFull(false) {
    constructMMR(zFindRelevant);
}

TxPoWTreeNode::TxPoWTreeNode(std::unique_ptr<TxBlock> zTxBlock)
    : TxPoWTreeNode(std::move(zTxBlock), true) {}

TxPoWTreeNode::TxPoWTreeNode(std::unique_ptr<TxBlock> zTxBlock, bool zFindRelevant)
    : mTxBlock(std::move(zTxBlock)),
      mParent(),
      mChildren(),
      mTotalWeight(MiniNumber::ZERO()),
      mMMR(nullptr),
      mCoins(),
      mRelevantMMRCoins(),
      mComputedRelevantCoins(),
      mHaveCheckedFull(false) {
    constructMMR(zFindRelevant);
}

// Used in tests
TxPoWTreeNode::TxPoWTreeNode(const TxPoW& zTestTxPoW)
    : mTxBlock(std::make_unique<TxBlock>(zTestTxPoW)),
      mParent(),
      mChildren(),
      mTotalWeight(MiniNumber::ZERO()),
      mMMR(std::make_unique<MMR>()),
      mCoins(),
      mRelevantMMRCoins(),
      mComputedRelevantCoins(),
      mHaveCheckedFull(false) {
    // Java test ctor didn't construct MMR from TxBlock
}

// Special members definitions (PIMPL fix)
TxPoWTreeNode::~TxPoWTreeNode() = default;
TxPoWTreeNode::TxPoWTreeNode(TxPoWTreeNode&&) noexcept = default;
TxPoWTreeNode& TxPoWTreeNode::operator=(TxPoWTreeNode&&) noexcept = default;

void TxPoWTreeNode::constructMMR(bool zFindRelevant) {
    // Validate TxBlock before using it
    if (!mTxBlock) {
        MinimaLogger::log("DEBUG ERROR, constructMMR: mTxBlock is null!");
        mMMR = std::make_unique<MMR>();
        return;
    }

    // What Block Time are we
    MiniNumber block = mTxBlock->getTxPoW().getBlockNumber();

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG: constructMMR START for Block: " + block.toString()
    // );

    // Create a new MMR
    mMMR = std::make_unique<MMR>();
    mMMR->setBlockTime(block);

    // Get the Wallet
    org::minima::database::wallet::Wallet* wallet = nullptr;
    if (zFindRelevant) {
        wallet = &org::minima::database::MinimaDB::getDB()->getWallet();
    }

    // Add all the peaks
    const std::vector<MMREntry>& peaks = mTxBlock->getPreviousPeaks();

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG: constructMMR Block: " + block.toString() +
    //     " found " + std::to_string(peaks.size()) + " previous peaks."
    // );

    for (const MMREntry& peak : peaks) {
        const MMRData* pdata = peak.getMMRData();
        if (pdata) {
            mMMR->setEntry(peak.getRow(), peak.getEntryNumber(), *pdata);
        }
    }

    // Calculate the Entry Number
    mMMR->calculateEntryNumberFromPeaks();

    // Has the balance changed
    bool balancechange = false;

    // Update spent coins from input proofs
    const std::vector<CoinProof>& spentcoins = mTxBlock->getInputCoinProofs();

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG: constructMMR Block: " + block.toString() +
    //     " processing " + std::to_string(spentcoins.size()) + " spent coins (inputs)"
    // );

    for (const CoinProof& input : spentcoins) {
        // Which entry in the MMR
        MMREntryNumber entrynumber = input.getCoin().getMMREntryNumber();

        // org::minima::utils::MinimaLogger::log(
        //     "DEBUG: constructMMR Block: " + block.toString() +
        //     " SPENDING Entry: " + entrynumber.toString()
        // );

        // A NEW copy of the spent coin - and set spent
        std::unique_ptr<Coin> spc = input.getCoin().deepCopy();
        Coin spentcoin = std::move(*spc);
        spentcoin.setSpent(true);

        // Create the MMRData
        std::unique_ptr<MMRData> mmrdata = MMRData::CreateMMRDataLeafNode(spentcoin, MiniNumber::ZERO());

        // Update the MMR
        mMMR->updateEntry(entrynumber, input.getMMRProof(), *mmrdata);

        // Add to all coins for this block
        mCoins.emplace_back(std::move(spentcoin));

        // Relevant?
        if (zFindRelevant && wallet) {
            const Coin& last = mCoins.back();
            if (checkRelevant(last, *wallet)) {
                mRelevantMMRCoins.push_back(entrynumber);

                // Message JSON
                JSONObject coinjson = last.toJSON(true);
                org::minima::utils::MinimaLogger::log(std::string("NEW Spent Coin : ") + coinjson.toString());

                // Send a message
                auto data = std::make_shared<JSONObject>();
                data->put("relevant", true);
                data->put("txblockid", mTxBlock->getTxPoW().getTxPoWID());
                data->put("txblock", block.toString());
                data->put("spent", true);
                data->put("coin", coinjson);

                // Post it
                org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NEWCOIN, *data);

                balancechange = true;
            }

            // Check the Coin Notify Details
            std::string coinaddress = last.getAddress().to0xString();
            if (org::minima::database::MinimaDB::getDB()->checkCoinNotify(coinaddress)) {
                JSONObject coinjson = last.toJSON(true);

                auto data = std::make_shared<JSONObject>();
                data->put("address", coinaddress);
                data->put("txblockid", mTxBlock->getTxPoW().getTxPoWID());
                data->put("txblock", block.toString());
                data->put("spent", true);
                data->put("coin", coinjson);

                org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCOIN, *data);
            }
        }
    }

    // Add all the newly created coins
    const std::vector<Coin>& outputs = mTxBlock->getOutputCoins();

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG: constructMMR Block: " + block.toString() +
    //     " processing " + std::to_string(outputs.size()) + " new coins (outputs)"
    // );

    for (const Coin& output : outputs) {
        // Where are we in the MMR
        MMREntryNumber entrynumber = mMMR->getEntryNumber();

        // org::minima::utils::MinimaLogger::log(
        //     "DEBUG: constructMMR Block: " + block.toString() +
        //     " ADDING Entry: " + entrynumber.toString()
        // );

        // Create new coin information - unspent
        std::unique_ptr<Coin> ncptr = output.deepCopy();
        Coin newcoin = std::move(*ncptr);
        newcoin.setMMREntryNumber(entrynumber);
        newcoin.setBlockCreated(block);
        newcoin.setSpent(false);

        // Create the MMRData
        std::unique_ptr<MMRData> mmrdata = MMRData::CreateMMRDataLeafNode(newcoin, output.getAmount());

        // Add to the MMR
        mMMR->addEntry(*mmrdata);

        // Add to this block list
        mCoins.emplace_back(std::move(newcoin));
        const Coin& last = mCoins.back();

        // Relevant?
        if (zFindRelevant && wallet) {
            if (checkRelevant(output, *wallet)) {
                mRelevantMMRCoins.push_back(entrynumber);

                // Message
                JSONObject coinjson = last.toJSON(true);

                // Did we remove the state?
                if (!last.storeState()) {
                    const auto* removedstate = mTxBlock->removedState(last.getCoinID().to0xString());
                    if (removedstate != nullptr) {
                        coinjson.put("state", ConvertStateVecToJSON(*removedstate));
                    }
                }

                org::minima::utils::MinimaLogger::log(std::string("NEW Unspent Coin : ") + coinjson.toString());

                // Send a message
                auto data = std::make_shared<JSONObject>();
                data->put("relevant", true);
                data->put("txblockid", mTxBlock->getTxPoW().getTxPoWID());
                data->put("txblock", block.toString());
                data->put("spent", false);
                data->put("coin", coinjson);

                org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NEWCOIN, *data);

                balancechange = true;
            }

            // Check Coin Notify
            std::string coinaddress = last.getAddress().to0xString();
            if (org::minima::database::MinimaDB::getDB()->checkCoinNotify(coinaddress)) {
                JSONObject coinjson = last.toJSON(true);

                if (!last.storeState()) {
                    const auto* removedstate = mTxBlock->removedState(last.getCoinID().to0xString());
                    if (removedstate != nullptr) {
                        coinjson.put("state", ConvertStateVecToJSON(*removedstate));
                    }
                }

                auto data = std::make_shared<JSONObject>();
                data->put("address", coinaddress);
                data->put("txblockid", mTxBlock->getTxPoW().getTxPoWID());
                data->put("txblock", block.toString());
                data->put("spent", false);
                data->put("coin", coinjson);

                org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCOIN, *data);
            }
        }
    }

    // All done!
    mMMR->finalizeSet();

    // org::minima::utils::MinimaLogger::log(
    //     "DEBUG: constructMMR FINISH for Block: " + block.toString() +
    //     " Final MMR Entry Number: " + mMMR->getEntryNumber().toString()
    // );

    // Calculate the relevant coins
    if (zFindRelevant) {
        calculateRelevantCoins();
    }

    // Notify balance change
    if (balancechange) {
        auto msg = std::make_shared<org::minima::utils::messages::Message>(
            org::minima::system::Main::MAIN_BALANCE
        );
        org::minima::system::Main::getInstance()->PostMessage(msg);
    }
}

bool TxPoWTreeNode::checkRelevant(const Coin& zCoin, org::minima::database::wallet::Wallet& zWallet) const {
    // Is the coin relevant to us (address-based)
    if (zWallet.isAddressRelevant(zCoin.getAddress().to0xString())) {
        return true;
    }

    // Are any of the state variables relevant to us
    const auto& state = zCoin.getState();
    for (const auto& svp : state) {
        const StateVariable& sv = *svp;
        if (sv.getType().isEqual(StateVariable::STATETYPE_HEX)) {
            std::string svstr = sv.toString();
            // Custom scripts have no public key
            if (zWallet.isAddressRelevant(svstr) || zWallet.isKeyRelevant(svstr)) {
                return true;
            }
        }
    }

    return false;
}

void TxPoWTreeNode::calculateRelevantCoins() {
    // Clear and start again
    mComputedRelevantCoins.clear();

    // Cycle through both lists
    for (const Coin& coin : mCoins) {
        // Cycle relevant entries
        for (const MMREntryNumber& relentry : mRelevantMMRCoins) {
            if (coin.getMMREntryNumber().isEqual(relentry)) {
                std::unique_ptr<Coin> cp = coin.deepCopy();
                mComputedRelevantCoins.emplace_back(std::move(*cp));
                break;
            }
        }
    }
}

TxBlock& TxPoWTreeNode::getTxBlock() {
    return *mTxBlock;
}

const TxBlock& TxPoWTreeNode::getTxBlock() const {
    return *mTxBlock;
}

TxPoW& TxPoWTreeNode::getTxPoW() {
    return const_cast<TxPoW&>(mTxBlock->getTxPoW());
}

const TxPoW& TxPoWTreeNode::getTxPoW() const {
    return mTxBlock->getTxPoW();
}

MiniNumber TxPoWTreeNode::getBlockNumber() const {
    return getTxPoW().getBlockNumber();
}

MMR& TxPoWTreeNode::getMMR() {
    return *mMMR;
}

const MMR& TxPoWTreeNode::getMMR() const {
    return *mMMR;
}

const std::vector<Coin>& TxPoWTreeNode::getAllCoins() const {
    return mCoins;
}

void TxPoWTreeNode::addCoin(const Coin& zCoin) {
    mCoins.push_back(zCoin);
}

bool TxPoWTreeNode::isRelevantEntry(const MMREntryNumber& zMMREntryNumber) const {
    for (const auto& entry : mRelevantMMRCoins) {
        if (entry.isEqual(zMMREntryNumber)) {
            return true;
        }
    }
    return false;
}

const std::vector<Coin>& TxPoWTreeNode::getRelevantCoins() const {
    return mComputedRelevantCoins;
}

const std::vector<MMREntryNumber>& TxPoWTreeNode::getRelevantCoinsEntries() const {
    return mRelevantMMRCoins;
}

void TxPoWTreeNode::addRelevantCoin(const MMREntryNumber& zEntry) {
    if (!isRelevantEntry(zEntry)) {
        mRelevantMMRCoins.push_back(zEntry);
    }
}

void TxPoWTreeNode::removeRelevantCoin(const MMREntryNumber& zEntry) {
    std::vector<MMREntryNumber> newRelevant;
    newRelevant.reserve(mRelevantMMRCoins.size());
    for (const auto& entry : mRelevantMMRCoins) {
        if (!entry.isEqual(zEntry)) {
            newRelevant.push_back(entry);
        }
    }
    mRelevantMMRCoins = std::move(newRelevant);
}

void TxPoWTreeNode::addChildNode(const std::shared_ptr<TxPoWTreeNode>& zTxPoWTreeNode) {
    // Set the parent
    zTxPoWTreeNode->setParent(shared_from_this());

    // Set the MMR parent
    zTxPoWTreeNode->getMMR().setParent(mMMR.get());

    // Add to children
    mChildren.push_back(zTxPoWTreeNode);
}

const std::vector<std::shared_ptr<TxPoWTreeNode>>& TxPoWTreeNode::getChildren() const {
    return mChildren;
}

void TxPoWTreeNode::setParent(const std::shared_ptr<TxPoWTreeNode>& zTxPoWTreeNode) {
    mParent = zTxPoWTreeNode;
}

std::shared_ptr<TxPoWTreeNode> TxPoWTreeNode::getParent() const {
    return mParent.lock();
}

std::shared_ptr<TxPoWTreeNode> TxPoWTreeNode::getParent(int zBlocks) const {
    std::shared_ptr<TxPoWTreeNode> parent = std::const_pointer_cast<TxPoWTreeNode>(shared_from_this());
    int counter = 0;
    while (counter < zBlocks && parent && parent->getParent()) {
        parent = parent->getParent();
        ++counter;
    }
    return parent;
}

std::shared_ptr<TxPoWTreeNode> TxPoWTreeNode::getPastNode(const MiniNumber& zBlockNumber) const {
    std::shared_ptr<TxPoWTreeNode> parent = std::const_pointer_cast<TxPoWTreeNode>(shared_from_this());
    while (parent) {
        if (parent->getTxPoW().getBlockNumber().isEqual(zBlockNumber)) {
            return parent;
        }
        parent = parent->getParent();
    }
    return parent;
}

void TxPoWTreeNode::copyParentRelevantCoins() {
    // Copy all the MMR Coins from parent
    auto parent = getParent();
    if (!parent) {
        return;
    }

    std::vector<std::shared_ptr<Coin>> unspentcoins =
        org::minima::system::brains::TxPoWSearcher::getAllRelevantUnspentCoins(parent);

    // We may be adding...
    mMMR->setFinalized(false);

    // Copy all to the new root
    for (const auto& coinptr : unspentcoins) {
        if (!coinptr) continue;

        // Which entry is this
        MMREntryNumber entry = coinptr->getMMREntryNumber();

        // Get the MMRData from an MMREntry local variable to keep lifetime valid
        MMREntry entryObj = mMMR->getEntry(0, entry);
        const MMRData* dataPtr = entryObj.getMMRData();
        if (!dataPtr) continue;
        MMRData data(dataPtr->getData(), dataPtr->getValue());

        // Get the entry proof
        MMRProof proof = mMMR->getProofToPeak(coinptr->getMMREntryNumber());

        // Now add it..
        mMMR->updateEntry(entry, proof, data);

        // Add to all coins..
        std::unique_ptr<Coin> cp = coinptr->deepCopy();
        mCoins.emplace_back(std::move(*cp));

        // And add to our list of relevant coins..
        mRelevantMMRCoins.push_back(entry);
    }

    // MMR remains unchanged.. Re-finalize
    mMMR->setFinalized(true);

    // Recalculate the relevant coins
    calculateRelevantCoins();
}

void TxPoWTreeNode::clearParent() {
    // No TreeNode
    mParent.reset();

    // No MMR Parent
    mMMR->clearParent();
}

void TxPoWTreeNode::setTotalWeight(const MiniNumber& zWeight) {
    mTotalWeight = zWeight;
}

void TxPoWTreeNode::addToTotalWeight(const MiniNumber& zWeight) {
    mTotalWeight = mTotalWeight.add(zWeight);
}

const MiniNumber& TxPoWTreeNode::getTotalWeight() const {
    return mTotalWeight;
}

bool TxPoWTreeNode::checkFullTxns(org::minima::database::txpowdb::TxPoWDB& zTxpDB) {
    // If we have already checked
    if (mHaveCheckedFull) {
        return true;
    }

    // Cycle through all TxPoW and see if we have them all
    std::vector<MiniData> txns = getTxPoW().getBlockTransactions();
    for (const MiniData& txn : txns) {
        bool exists = zTxpDB.exists(txn.to0xString());
        if (!exists) {
            return false;
        }
    }

    // We now know we have all the txns
    mHaveCheckedFull = true;
    return true;
}

void TxPoWTreeNode::writeDataStream(std::ostream& zOut) {
    // mTxBlock and mMMR
    mTxBlock->writeDataStream(zOut);
    mMMR->writeDataStream(zOut);

    // Coins
    int len = static_cast<int>(mCoins.size());
    MiniNumber::WriteToStream(zOut, len);
    for (auto& cmmr : mCoins) {
        cmmr.writeDataStream(zOut);
    }

    // Relevant entries
    len = static_cast<int>(mRelevantMMRCoins.size());
    MiniNumber::WriteToStream(zOut, len);
    for (auto& rel : mRelevantMMRCoins) {
        rel.writeDataStream(zOut);
    }
}

void TxPoWTreeNode::readDataStream(std::istream& zIn) {
    mChildren.clear();
    mTotalWeight = MiniNumber::ZERO();
    mParent.reset();
    mCoins.clear();
    mRelevantMMRCoins.clear();
    mComputedRelevantCoins.clear();
    mHaveCheckedFull = false;

    // TxBlock
    {
        std::unique_ptr<TxBlock> tb = TxBlock::ReadFromStream(zIn);
        mTxBlock = std::move(tb);
    }
    // MMR
    {
        std::unique_ptr<MMR> mmr = MMR::ReadFromStream(zIn);
        mMMR = std::move(mmr);
    }

    // Coins
    int len = MiniNumber::ReadFromStream(zIn).getAsInt();
    for (int i = 0; i < len; ++i) {
        std::unique_ptr<Coin> c = Coin::ReadFromStream(zIn);
        mCoins.emplace_back(std::move(*c));
    }

    // Relevant Entries
    len = MiniNumber::ReadFromStream(zIn).getAsInt();
    for (int i = 0; i < len; ++i) {
        MMREntryNumber en = MMREntryNumber::ReadFromStream(zIn);
        mRelevantMMRCoins.emplace_back(std::move(en));
    }

    calculateRelevantCoins();
}

std::unique_ptr<TxPoWTreeNode> TxPoWTreeNode::ReadFromStream(std::istream& zIn) {
    auto node = std::unique_ptr<TxPoWTreeNode>(new TxPoWTreeNode());
    node->readDataStream(zIn);
    return node;
}

void TxPoWTreeNode::CheckTxBlockForNotifyCoins(const TxBlock& zBlock) {
    const TxPoW& txp_const = zBlock.getTxPoW();
    TxPoW& txp = const_cast<TxPoW&>(txp_const); // getTransactions() may be non-const
    std::string blockid   = txp.getTxPoWID();
    std::string blocknum  = txp.getBlockNumber().toString();
    org::minima::database::MinimaDB* db = org::minima::database::MinimaDB::getDB();

    // Get the block transactions
    std::vector<std::string> transactions = txp.getTransactions();

    // This Block is now in the cascade
    auto blockdata = std::make_shared<JSONObject>();
    blockdata->put("txpow", txp.toJSON());
    org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCASCADEBLOCK, *blockdata);

    // Always add to the onChain DB
    db->getTxPoWDB().getOnChainDB()->addOnChainTxPoW(blockid, txp.getBlockNumber(), blockid);

    // Is THIS block a TXN..
    if (txp.isTransaction()) {
        auto data = std::make_shared<JSONObject>();
        data->put("txblockid", blockid);
        data->put("txblock", blocknum);
        data->put("txpowid", blockid);

        org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCASCADETXN, *data);
    }

    // Cycle through all the txns as they are NOW on chain permanently..
    for (const std::string& txn : transactions) {
        auto data = std::make_shared<JSONObject>();
        data->put("txblockid", blockid);
        data->put("txblock", blocknum);
        data->put("txpowid", txn);

        org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCASCADETXN, *data);

        // Add to the DB
        db->getTxPoWDB().getOnChainDB()->addOnChainTxPoW(blockid, txp.getBlockNumber(), txn);
    }

    // Get all the input coins..
    const std::vector<CoinProof>& inputs = zBlock.getInputCoinProofs();
    for (const CoinProof& cp : inputs) {
        const Coin& cc = cp.getCoin();

        std::string coinaddress = cc.getAddress().to0xString();
        if (db->checkCoinNotify(coinaddress)) {
            JSONObject coinjson = cc.toJSON(true);

            auto data = std::make_shared<JSONObject>();
            data->put("address", coinaddress);
            data->put("txblockid", blockid);
            data->put("txblock", blocknum);
            data->put("spent", true);
            data->put("coin", coinjson);

            org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCASCADECOIN, *data);
        }
    }

    // Get all the output coins
    const std::vector<Coin>& outputs = zBlock.getOutputCoins();
    for (const Coin& cc : outputs) {
        std::string coinaddress = cc.getAddress().to0xString();
        if (db->checkCoinNotify(coinaddress)) {
            JSONObject coinjson = cc.toJSON(true);

            if (!cc.storeState()) {
                const auto* removedstate = zBlock.removedState(cc.getCoinID().to0xString());
                if (removedstate != nullptr) {
                    coinjson.put("state", ConvertStateVecToJSON(*removedstate));
                }
            }

            auto data = std::make_shared<JSONObject>();
            data->put("address", coinaddress);
            data->put("txblockid", blockid);
            data->put("txblock", blocknum);
            data->put("spent", false);
            data->put("coin", coinjson);

            org::minima::system::Main::getInstance()->PostNotifyEvent(org::minima::system::Main::MAIN_NOTIFYCASCADECOIN, *data);
        }
    }
}

} // namespace txpowtree
} // namespace database
} // namespace minima
} // namespace org