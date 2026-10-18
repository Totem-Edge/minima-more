#pragma once

#include <memory>
#include <vector>
#include <cstdint>

// Forward declarations to avoid header cycles (Rule 10)
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace objects { namespace mmr {
    class MMR;
    class MMRProof;
    class MMREntryNumber;
    class MMRData;
} } } }

namespace org { namespace minima { namespace objects { namespace keys {
    class Winternitz;
    class SignatureProof;
} } } }

namespace org {
namespace minima {
namespace objects {
namespace keys {

class TreeKeyNode {
public:
    // Constructors
    TreeKeyNode(const org::minima::objects::base::MiniData& zPrivateSeed, int zSize);

    // Destructor and special members (PIMPL fix - Rule 7)
    virtual ~TreeKeyNode();
    TreeKeyNode(TreeKeyNode&&) noexcept;
    TreeKeyNode& operator=(TreeKeyNode&&) noexcept;

    // Disable copy
    TreeKeyNode(const TreeKeyNode&) = delete;
    TreeKeyNode& operator=(const TreeKeyNode&) = delete;

    // Accessors and operations (functional equivalents)
    TreeKeyNode& getChild(int zChild);

    // Returns a copy of the public key (Java returned by value)
    org::minima::objects::base::MiniData getPublicKey() const;

    // Returns a reference to the WOTS key (Java returned reference to object)
    Winternitz& getWOTSKey(int zKeyNum);

    // Returns a proof by value (Java returned object by value)
    org::minima::objects::mmr::MMRProof getProof(int zKeyNum);

    bool childSigExists() const;

    void setParentChildSig(const SignatureProof& zSignature);

    // Returns reference; throws if not set (Java could return null; caller checks childSigExists first)
    const SignatureProof& getParentChildSig() const;

private:
    int mSize;

    std::unique_ptr<org::minima::objects::mmr::MMR> mTree;

    std::vector<std::unique_ptr<TreeKeyNode>> mChildren;
    std::vector<std::unique_ptr<Winternitz>> mKeys;

    std::unique_ptr<org::minima::objects::base::MiniData> mChildSeed;
    std::unique_ptr<org::minima::objects::base::MiniData> mPublicKey;

    std::unique_ptr<SignatureProof> mParentChildSig;
};

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org