#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

// Forward declarations for MMR-related classes
namespace org { namespace minima { namespace objects { namespace mmr {
class MMREntry;
class MMREntryNumber;
class MMRData;
class MMRProof;
} } } }

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MMR : public org::minima::utils::Streamable {
public:
    // Constants
    static constexpr int MAXROWS = 256;

    // Constructors
    MMR();
    explicit MMR(MMR* zParent);

    // Destructor and special members (needed due to unique_ptr to forward-declared types)
    virtual ~MMR();
    MMR(MMR&&) noexcept;
    MMR& operator=(MMR&&) noexcept;

    // Delete copy ops
    MMR(const MMR&) = delete;
    MMR& operator=(const MMR&) = delete;

    // Core functionality
    void calculateEntryNumberFromPeaks();

    org::minima::utils::json::JSONObject toJSON();
    org::minima::utils::json::JSONObject toJSON(bool zEntries);

    void finalizeSet();
    void setFinalized(bool zFinalized);
    bool isFinalized() const;

    void setBlockTime(const org::minima::objects::base::MiniNumber& zTime);
    org::minima::objects::base::MiniNumber getBlockTime() const;

    const MMREntryNumber& getEntryNumber() const;

    void clearParent();
    void setParent(MMR* zMMR);
    MMR* getParent() const;

    int getTotalEntries() const;
    const std::unordered_map<std::string, std::unique_ptr<MMREntry>>& getAllEntries() const;

    // Set/get entries
    MMREntry setEntry(int zRow, const MMREntryNumber& zEntry, const MMRData& zData);
    MMREntry getEntry(int zRow, const MMREntryNumber& zEntry);
    MMREntry getEntry(int zRow, const MMREntryNumber& zEntry, const org::minima::objects::base::MiniNumber& zMaxBack) const;

    // Add/Update
    MMREntry addEntry(const MMRData& zData);
    void updateEntry(const MMREntryNumber& zEntry, const MMRProof& zOldProof, const MMRData& zNewData);

    // Proofs
    MMRProof getProof(const MMREntryNumber& zEntryNumber);
    MMRProof getProofToPeak(const MMREntryNumber& zEntryNumber);

    // Peaks and root
    std::vector<MMREntry> getPeaks() const;
    std::unique_ptr<MMRData> getRoot() const;

    // Proof checks
    bool checkProof(const MMRData& zMMRData, const MMRProof& zProof) const;
    bool checkProofTimeValid(const MMREntryNumber& zEntry, const MMRData& zMMRData, const MMRProof& zProof) const;    // Find parent MMR at a specific time
    const MMR* getParentAtTime(const org::minima::objects::base::MiniNumber& zTime) const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    static std::unique_ptr<MMR> ReadFromStream(std::istream& in);

    // Pruning and unspendable scanning
    void pruneTree();
    const std::unordered_set<std::string>& getPrunedUnspendableCoins() const;
    void scanUnspendableTree();

protected:
    // Protected helper to match Java visibility
    MMRProof getPeakToRoot(const MMRData& zPeak);

private:
    // Helpers
    std::string getHashTableEntry(int zRow, const MMREntryNumber& zEntry) const;
    void addHashTableEntry(const MMREntry& zEntry);
    void removeHashTableEntry(const MMREntry& zEntry);

    void prune(const MMREntry& zStartNode);
    bool scanUnspendable(const MMREntry& zStartNode);

    std::unique_ptr<MMRData> getRootInternal() const;

    // Data members
    org::minima::objects::base::MiniNumber mBlockTime { 0 };

    MMR* mParent { nullptr };

    bool mUseMegaMMR { false };

    std::unique_ptr<MMREntryNumber> mEntryNumber; // initialized in ctor

    std::unordered_map<std::string, std::unique_ptr<MMREntry>> mSetEntries;

    int mMaxRow { 0 };

    std::vector<std::unique_ptr<MMREntry>> mMaxEntries; // size MAXROWS

    bool mFinalized { false };
    std::unique_ptr<MMRData> mFinalizedRoot;
    std::vector<std::unique_ptr<MMREntry>> mFinalizedPeaks;

    std::unordered_set<std::string> mPrunedCoins;
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org