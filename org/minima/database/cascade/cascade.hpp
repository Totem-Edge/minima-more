#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <ostream>
#include <istream>
#include <vector>
#include <shared_mutex>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

// Forward declarations
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace database { namespace cascade { class CascadeNode; } } } }

namespace org {
namespace minima {
namespace database {
namespace cascade {

class Cascade : public org::minima::utils::Streamable {
public:
    Cascade();
    virtual ~Cascade();

    Cascade(const Cascade&) = delete;
    Cascade& operator=(const Cascade&) = delete;
    Cascade(Cascade&&) = delete;
    Cascade& operator=(Cascade&&) = delete;

    void addToTip(const org::minima::objects::TxPoW& zTxPoW);
    void cascadeChain();

    std::shared_ptr<CascadeNode> getTip() const;
    org::minima::objects::base::MiniNumber getTotalWeight() const;
    int getLength() const;

    std::unique_ptr<Cascade> deepCopy() const;
    std::string printCascade() const;

    void loadDB(const std::filesystem::path& zFile);
    void saveDB(const std::filesystem::path& zFile);

    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    static std::unique_ptr<Cascade> convertMiniDataVersion(const org::minima::objects::base::MiniData& zCascData);
    static std::unique_ptr<Cascade> ReadFromStream(std::istream& in);

    static bool checkCascadeCorrect(const Cascade& zCascade);
    static bool checkPastNodeExists(CascadeNode* zCascadeNode, const std::string& zTxPoWID);

    // Thread-safe chain walk: given a node from this cascade, return its parent as a shared_ptr.
    // Returns nullptr if the parent is no longer in the cascade or node is null.
    // Each call acquires a shared_lock internally.
    std::shared_ptr<CascadeNode> getParent(const std::shared_ptr<CascadeNode>& zNode) const;

private:
    mutable std::shared_mutex mMutex;
    std::vector<std::shared_ptr<CascadeNode>> mNodes;
    std::shared_ptr<CascadeNode> mTip;
    org::minima::objects::base::MiniNumber mTotalWeight;
    bool hasBlock(const std::string& zTxPoWID) const;

    // Lock-free internal parent lookup. Caller must already hold mMutex
    // (any lock level) — avoids recursive locking when chaining.
    std::shared_ptr<CascadeNode> getParentNoLock(const std::shared_ptr<CascadeNode>& zNode) const;
};

} // namespace cascade
} // namespace database
} // namespace minima
} // namespace org
