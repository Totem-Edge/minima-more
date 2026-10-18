#pragma once

#include <memory>
#include <vector>
#include <stdexcept>

#include "org/minima/objects/base/mini_data.hpp"

// Namespaced forward declarations (Rule 10)
namespace org { namespace minima { namespace objects { namespace keys {
    class TreeKeyNode;
    class Signature;
    class SignatureProof;
    class Winternitz;
} } } }
namespace org { namespace minima { namespace objects { namespace mmr {
    class MMRProof;
} } } }

namespace org {
namespace minima {
namespace objects {
namespace keys {

class TreeKey {
public:
    // Constants
    static constexpr int MAX_KEY_LEVELS       = 8;
    static constexpr int DEFAULT_KEYSPERLEVEL = 64;
    static constexpr int DEFAULT_LEVELS       = 3;

    // Factory
    static TreeKey createDefault(const org::minima::objects::base::MiniData& zPrivateSeed);

    // Constructors / Destructor
    TreeKey(); // default
    TreeKey(const org::minima::objects::base::MiniData& zPrivateSeed, int zKeyNum, int zLevels);

    virtual ~TreeKey();
    TreeKey(TreeKey&&) noexcept;
    TreeKey& operator=(TreeKey&&) noexcept;

    TreeKey(const TreeKey&) = delete;
    TreeKey& operator=(const TreeKey&) = delete;

    // Public API
    void setPublicKey(const org::minima::objects::base::MiniData& zPublicKey);

    org::minima::objects::base::MiniData getPublicKey() const;
    org::minima::objects::base::MiniData getPrivateKey() const;

    int getMaxUses() const;
    int getUses() const;
    void setUses(int zUses);

    int getSize() const;
    int getDepth() const;

    // Signing and verification
    Signature sign(const org::minima::objects::base::MiniData& zData);
    bool verify(const org::minima::objects::base::MiniData& zData, const Signature& zSignature) const;

private:
    // Helper: base conversion to select node indices per level
    static std::vector<int> baseConversion(int zNum, int zBase, int zLevels);

    // Members
    std::unique_ptr<TreeKeyNode> mRoot;

    int mLevels       = 0;
    int mKeysPerLevel = 0;
    int mUses         = 0;
    int mMaxUses      = 0;

    org::minima::objects::base::MiniData mPrivateSeed;
    org::minima::objects::base::MiniData mPublicKey;
};

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org