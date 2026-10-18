#include "org/minima/objects/keys/tree_key_node.hpp"

#include <stdexcept>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp" // <--- ADDED THIS INCLUDE
#include "org/minima/utils/crypto.hpp"

// Include keys dependencies (full headers)
#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/objects/keys/signature_proof.hpp"

namespace org {
namespace minima {
namespace objects {
namespace keys {

// Destructor and special members definitions (Rule 7)
TreeKeyNode::~TreeKeyNode() = default;
TreeKeyNode::TreeKeyNode(TreeKeyNode&&) noexcept = default;
TreeKeyNode& TreeKeyNode::operator=(TreeKeyNode&&) noexcept = default;

TreeKeyNode::TreeKeyNode(const org::minima::objects::base::MiniData& zPrivateSeed, int zSize)
    : mSize(zSize)
{
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::mmr::MMR;
    using org::minima::objects::mmr::MMRData;

    if (mSize <= 0) {
        throw std::invalid_argument("TreeKeyNode size must be > 0");
    }

    // Hash it again.. so the children have a different base
    {
        MiniData privcopy(zPrivateSeed);
        MiniData childseed = org::minima::utils::Crypto::getInstance().hashObject(privcopy);
        mChildSeed = std::make_unique<MiniData>(childseed);
    }

    // Prepare children vector with null entries
    mChildren.resize(static_cast<std::size_t>(mSize));

    // Create a new MMR Tree
    mTree = std::make_unique<MMR>();

    // Create the Keys
    mKeys.resize(static_cast<std::size_t>(mSize));

    // Add
    for (int i = 0; i < zSize; ++i) {
        // Create a new deterministic private seed for the key..
        MiniNumber idx(i);
        MiniData privcopy(zPrivateSeed);
        MiniData seed = org::minima::utils::Crypto::getInstance().hashAllObjects({ &idx, &privcopy });

        // Create NEW SingleKey
        auto wots = std::make_unique<Winternitz>(seed);

        // What is the PublicKey
        MiniData pubkey = wots->getPublicKey();

        // Create the MMRData entry
        std::unique_ptr<MMRData> pubentry = MMRData::CreateMMRDataLeafNode(pubkey, MiniNumber::ZERO());

        // Add this to the MMR..
        mTree->addEntry(*pubentry);

        // Keep this in an array - for quick retrieval
        mKeys[static_cast<std::size_t>(i)] = std::move(wots);
    }

    // What is the Public Key.. root of the MMR..
    {
        std::unique_ptr<MMRData> root = mTree->getRoot();
        if (!root) {
            throw std::runtime_error("MMR root is null when computing public key");
        }
        mPublicKey = std::make_unique<MiniData>(root->getData());
    }
}

TreeKeyNode& TreeKeyNode::getChild(int zChild) {
    if (zChild < 0 || zChild >= mSize) {
        throw std::out_of_range("TreeKeyNode::getChild index out of range");
    }

    std::size_t idx = static_cast<std::size_t>(zChild);
    if (!mChildren[idx]) {
        using org::minima::objects::base::MiniData;
        using org::minima::objects::base::MiniNumber;

        // Create a new deterministic private seed for child tree
        MiniNumber nidx(zChild);
        MiniData seed = org::minima::utils::Crypto::getInstance().hashAllObjects({ &nidx, mChildSeed.get() });

        // Now create a new Child
        mChildren[idx] = std::make_unique<TreeKeyNode>(seed, mSize);
    }

    return *mChildren[idx];
}

org::minima::objects::base::MiniData TreeKeyNode::getPublicKey() const {
    if (!mPublicKey) {
        throw std::runtime_error("Public key not initialized");
    }
    return *mPublicKey; // return by value
}

Winternitz& TreeKeyNode::getWOTSKey(int zKeyNum) {
    if (zKeyNum < 0 || zKeyNum >= mSize) {
        throw std::out_of_range("TreeKeyNode::getWOTSKey index out of range");
    }
    std::size_t idx = static_cast<std::size_t>(zKeyNum);
    if (!mKeys[idx]) {
        throw std::runtime_error("WOTS key not initialized at index");
    }
    return *mKeys[idx];
}

org::minima::objects::mmr::MMRProof TreeKeyNode::getProof(int zKeyNum) {
    if (zKeyNum < 0 || zKeyNum >= mSize) {
        throw std::out_of_range("TreeKeyNode::getProof index out of range");
    }
    org::minima::objects::mmr::MMREntryNumber en(zKeyNum);
    return mTree->getProof(en);
}

bool TreeKeyNode::childSigExists() const {
    return static_cast<bool>(mParentChildSig);
}

void TreeKeyNode::setParentChildSig(const SignatureProof& zSignature) {
    // Deep-copy by reconstructing from components since SignatureProof is non-copyable
    mParentChildSig = std::make_unique<SignatureProof>(
        zSignature.getPublicKey(),
        zSignature.getSignature(),
        zSignature.getProof()
    );
}

const SignatureProof& TreeKeyNode::getParentChildSig() const {
    if (!mParentChildSig) {
        throw std::runtime_error("Parent-child signature not set");
    }
    return *mParentChildSig;
}

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org
