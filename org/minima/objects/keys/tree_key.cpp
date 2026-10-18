#include "org/minima/objects/keys/tree_key.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "org/minima/objects/keys/tree_key_node.hpp"
#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/signature_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace objects {
namespace keys {

// Destructor and move operations (PITFALL 1)
TreeKey::~TreeKey() = default;
TreeKey::TreeKey(TreeKey&&) noexcept = default;
TreeKey& TreeKey::operator=(TreeKey&&) noexcept = default;

// Factory
TreeKey TreeKey::createDefault(const org::minima::objects::base::MiniData& zPrivateSeed) {
    return TreeKey(zPrivateSeed, DEFAULT_KEYSPERLEVEL, DEFAULT_LEVELS);
}

// Constructors
TreeKey::TreeKey() = default;

TreeKey::TreeKey(const org::minima::objects::base::MiniData& zPrivateSeed, int zKeyNum, int zLevels)
    : mLevels(zLevels),
      mKeysPerLevel(zKeyNum),
      mUses(0),
      mPrivateSeed(zPrivateSeed) {

    if (mLevels > MAX_KEY_LEVELS) {
        throw std::invalid_argument(
            "Too many key Levels " + std::to_string(mLevels) + " MAX:" + std::to_string(MAX_KEY_LEVELS));
    }

    // Use double pow and cast to int to mirror Java Math.pow semantics
    mMaxUses = static_cast<int>(std::pow(static_cast<double>(mKeysPerLevel), static_cast<double>(mLevels)));

    // Initialise root
    mRoot = std::make_unique<TreeKeyNode>(zPrivateSeed, mKeysPerLevel);

    // Get the Public Key
    mPublicKey = mRoot->getPublicKey();
}

// Public API
void TreeKey::setPublicKey(const org::minima::objects::base::MiniData& zPublicKey) {
    mPublicKey = zPublicKey;
}

org::minima::objects::base::MiniData TreeKey::getPublicKey() const {
    return mPublicKey;
}

org::minima::objects::base::MiniData TreeKey::getPrivateKey() const {
    return mPrivateSeed;
}

int TreeKey::getMaxUses() const {
    return mMaxUses;
}

int TreeKey::getUses() const {
    return mUses;
}

void TreeKey::setUses(int zUses) {
    mUses = zUses;
}

int TreeKey::getSize() const {
    return mKeysPerLevel;
}

int TreeKey::getDepth() const {
    return mLevels;
}

// Signing
Signature TreeKey::sign(const org::minima::objects::base::MiniData& zData) {
    using org::minima::objects::base::MiniData;
    using org::minima::utils::MinimaLogger;

    // MinimaLogger::log("DEBUG_TREEKEY: Starting sign() for data: " + zData.to0xString());
    // MinimaLogger::log("DEBUG_TREEKEY: Current uses: " + std::to_string(mUses) + " / " + std::to_string(mMaxUses));

    // Check range
    if (mUses >= mMaxUses) {
        MinimaLogger::log("SERIOUS ERROR : MAX TREEKEYS USED @ " + mPublicKey.toString());
        mUses = 0;
    }

    // Get the Correct Node path
    std::vector<int> nodes = baseConversion(mUses, mKeysPerLevel, mLevels);
    // MinimaLogger::log("DEBUG_TREEKEY: Node path: [" + 
        // [&nodes]() {
        //     std::string s;
        //     for (size_t i = 0; i < nodes.size(); ++i) {
        //         if (i > 0) s += ", ";
        //         s += std::to_string(nodes[i]);
        //     }
        //     return s;
        // }() + "]");

    // All the signatures
    Signature signature;

    // Now get those Nodes
    TreeKeyNode* current = mRoot.get();
    int depth = 1;

    for (int keynum : nodes) {
        // MinimaLogger::log("DEBUG_TREEKEY: Processing depth " + std::to_string(depth) + ", keynum " + std::to_string(keynum));

        // Get the required key
        Winternitz* wots = &current->getWOTSKey(keynum);

        // The Public Key for this WOTS
        MiniData sigpubkey = wots->getPublicKey();
        // MinimaLogger::log("DEBUG_TREEKEY: WOTS pubkey: " + sigpubkey.to0xString());

        // Get the MMRProof
        auto proof_obj = current->getProof(keynum);

        // Is this the final node
        if (depth == mLevels) {
            // MinimaLogger::log("DEBUG_TREEKEY: Final depth - signing actual data");
            
            // Sign the actual Data
            MiniData sigdata = wots->sign(zData);
            // MinimaLogger::log("DEBUG_TREEKEY: Signature data length: " + std::to_string(sigdata.getLength()));

            // Create a signature object and add it
            auto sigproof = std::make_unique<SignatureProof>(sigpubkey, sigdata, proof_obj);
            // MinimaLogger::log("DEBUG_TREEKEY: Created final SignatureProof, root key: " + sigproof->getRootPublicKey().to0xString());
            signature.addSignatureProof(std::move(sigproof));

        } else {
            // MinimaLogger::log("DEBUG_TREEKEY: Intermediate depth - processing child");
            
            // Get the correct child node
            TreeKeyNode* child = &current->getChild(keynum);
            MiniData childPubKey = child->getPublicKey();
            // MinimaLogger::log("DEBUG_TREEKEY: Child pubkey: " + childPubKey.to0xString());

            // Do we need to sign it (only need to do this once if reused multiple times)
            if (!child->childSigExists()) {
                // MinimaLogger::log("DEBUG_TREEKEY: Child sig doesn't exist, creating it");
                
                // Get the child's Public Key
                MiniData data = child->getPublicKey();

                // Sign the root of the child tree
                MiniData sigdata = wots->sign(data);
                // MinimaLogger::log("DEBUG_TREEKEY: Signed child pubkey, sigdata length: " + std::to_string(sigdata.getLength()));

                // Create the signature object and cache it on the child
                SignatureProof childsig(sigpubkey, sigdata, proof_obj);
                // MinimaLogger::log("DEBUG_TREEKEY: Created child SignatureProof, root key: " + childsig.getRootPublicKey().to0xString());
                child->setParentChildSig(std::move(childsig));
            } else {
                // MinimaLogger::log("DEBUG_TREEKEY: Child sig already exists, reusing it");
            }

            // Get the parent child sig
            const SignatureProof& parentchild = child->getParentChildSig();
            // MinimaLogger::log("DEBUG_TREEKEY: Retrieved parent-child sig:");
            // MinimaLogger::log("  - Public key: " + parentchild.getPublicKey().to0xString());
            // MinimaLogger::log("  - Root key: " + parentchild.getRootPublicKey().to0xString());
            // MinimaLogger::log("  - Signature length: " + std::to_string(parentchild.getSignature().getLength()));

            // CRITICAL: Instead of creating a new SignatureProof, let's add a copy
            // This should match Java behavior more closely
            auto sigproof = std::make_unique<SignatureProof>(
                parentchild.getPublicKey(),
                parentchild.getSignature(),
                parentchild.getProof()
            );
            // MinimaLogger::log("DEBUG_TREEKEY: Created copy SignatureProof for addition");
            signature.addSignatureProof(std::move(sigproof));

            // New current node
            current = child;
        }

        depth++;
    }

    // One signature done
    mUses++;
    // MinimaLogger::log("DEBUG_TREEKEY: Signature complete, total proofs: " + std::to_string(signature.getAllSignatureProofs().size()));

    // Return that
    return signature;
}

// Verification
bool TreeKey::verify(const org::minima::objects::base::MiniData& zData, const Signature& zSignature) const {
    using org::minima::utils::MinimaLogger;

    // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Starting verification");
    // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Data to verify: " + zData.to0xString());
    // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Expected root pubkey: " + mPublicKey.to0xString());

    // Cycle through
    const auto& allproofs = zSignature.getAllSignatureProofs();
    int total = static_cast<int>(allproofs.size());
    // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Total signature proofs: " + std::to_string(total));
    
    if (total > MAX_KEY_LEVELS) {
        MinimaLogger::log("[!] INVALID KEY found with " + std::to_string(total) +
                          " levels MAX:" + std::to_string(MAX_KEY_LEVELS));
        return false;
    }

    for (int depth = 0; depth < total; ++depth) {
        const SignatureProof& sigproof = *allproofs[depth];
        
        // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Checking depth " + std::to_string(depth));
        // MinimaLogger::log("  - Public key: " + sigproof.getPublicKey().to0xString());
        // MinimaLogger::log("  - Root key: " + sigproof.getRootPublicKey().to0xString());

        // Check this root public key is the one we need
        if (depth == 0) {
            // Check this is the MAIN public Key
            if (!sigproof.getRootPublicKey().isEqual(mPublicKey)) {
                // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: FAILED - Root key mismatch at depth 0");
                return false;
            }
            // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Root key matches at depth 0");
        }

        // Is this the last Signature
        if (depth == total - 1) {
            // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Final depth - verifying actual data");
            // The LAST signature signs the actual DATA
            bool result = Winternitz::verify(sigproof.getPublicKey(), zData, sigproof.getSignature());
            // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Final verification result: " + std::string(result ? "SUCCESS" : "FAILED"));
            return result;
        } else {
            // Any Signature but the last signs the child root public key
            const SignatureProof& childsig = *allproofs[depth + 1];
            // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Intermediate depth - verifying child root key");
            // MinimaLogger::log("  - Child root key: " + childsig.getRootPublicKey().to0xString());

            // Check this is what is signed
            if (!Winternitz::verify(sigproof.getPublicKey(), childsig.getRootPublicKey(), sigproof.getSignature())) {
                // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: FAILED - Child root key verification failed at depth " + std::to_string(depth));
                return false;
            }
            // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Child root key verified at depth " + std::to_string(depth));
        }
    }

    // MinimaLogger::log("DEBUG_TREEKEY_VERIFY: Unexpected fallthrough - returning false");
    return false;
}

// Private helpers
std::vector<int> TreeKey::baseConversion(int zNum, int zBase, int zLevels) {
    std::vector<int> ret;

    int counter = zNum;
    while (counter != 0) {
        int div = counter / zBase;
        int remain = counter - (div * zBase);
        ret.push_back(remain);
        counter = div;
    }

    // Pad to zLevels with zeros
    int sizediff = zLevels - static_cast<int>(ret.size());
    for (int i = 0; i < sizediff; ++i) {
        ret.push_back(0);
    }

    // Reverse
    std::reverse(ret.begin(), ret.end());

    return ret;
}

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org