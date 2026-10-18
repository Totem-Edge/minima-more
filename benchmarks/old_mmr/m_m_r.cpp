#include "org/minima/objects/mmr/m_m_r.hpp"

#include <sstream>
#include <cmath>
#include <utility>
#include <any>
#include <optional> 
#include <boost/multiprecision/cpp_int.hpp>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp" 
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

using org::minima::database::MinimaDB;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::params::GeneralParams;
using org::minima::utils::MinimaLogger;

namespace org {
namespace minima {
namespace objects {
namespace mmr {

// Destructor and special members definitions
MMR::~MMR() = default;
MMR::MMR(MMR&&) noexcept = default;
MMR& MMR::operator=(MMR&&) noexcept = default;

MMR::MMR()
    : mParent(nullptr)
    , mUseMegaMMR(false)
    , mEntryNumber(std::make_unique<MMREntryNumber>(boost::multiprecision::cpp_int(0)))
    , mSetEntries()
    , mMaxRow(0)
    , mMaxEntries(MAXROWS)
    , mFinalized(false)
{
    // Ensure mMaxEntries is initialized to nullptrs
    for (int i = 0; i < MAXROWS; ++i) {
        mMaxEntries[i] = nullptr;
    }
}

MMR::MMR(MMR* zParent)
    : MMR() // delegate to default to initialize members
{
    if (zParent != nullptr) {
        // Parent MMRSet
        setParent(zParent);

        if (!zParent->isFinalized()) {
            // Finalize the parent
            zParent->finalizeSet();
        }

        // Set the block time.. 1 more than parent
        setBlockTime(zParent->getBlockTime().add(MiniNumber::ONE()));

        // Add the peaks and calculate total entries (binary represented by peaks)
        boost::multiprecision::cpp_int tot = 0;

        std::vector<MMREntry> peaks = zParent->getPeaks();
        for (const MMREntry& peak : peaks) {
            setEntry(peak.getRow(), peak.getEntryNumber(), *peak.getMMRData()); 
            
            // ### FIX: CALCULATE THE TOTAL ENTRY NUMBER ###
            boost::multiprecision::cpp_int peakValue = 1;
            peakValue <<= peak.getRow(); // This is 2^row
            tot += peakValue;
        }

        // Set the Entry Number
        mEntryNumber = std::make_unique<MMREntryNumber>(tot);

        // Check!
        if (!mEntryNumber->isEqual(zParent->getEntryNumber())) {
            MinimaLogger::log("SERIOUS ERROR - Entry Number Mismatch! "
                              + mEntryNumber->toString() + "/"
                              + zParent->getEntryNumber().toString());
        }
    }
}

void MMR::calculateEntryNumberFromPeaks() {
    boost::multiprecision::cpp_int tot = 0;

    std::vector<MMREntry> peaks = getPeaks();
    for (const MMREntry& peak : peaks) {
        setEntry(peak.getRow(), peak.getEntryNumber(), *peak.getMMRData()); 
        // This logic is from MMR(MMR* zParent) constructor
        boost::multiprecision::cpp_int peakValue = 1;
        peakValue <<= peak.getRow(); // This is 2^row
        tot += peakValue;
    }

    mEntryNumber = std::make_unique<MMREntryNumber>(tot);
}

org::minima::utils::json::JSONObject MMR::toJSON() {
    return toJSON(true);
}

org::minima::utils::json::JSONObject MMR::toJSON(bool zEntries) {
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;

    JSONObject ret;
    ret.put("block", mBlockTime.toString());
    ret.put("entrynumber", mEntryNumber ? mEntryNumber->toString() : std::string("0"));
    ret.put("size", static_cast<int>(mSetEntries.size()));

    if (zEntries) {
        JSONArray jentry;
        for (const auto& kv : mSetEntries) {
            const MMREntry& entry = *kv.second;
            jentry.add(entry.toJSON());
        }
        ret.put("entries", jentry);
    }

    ret.put("maxrow", mMaxRow);
    {
        JSONArray maxentry;
        for (const auto& up : mMaxEntries) {
            if (up) {
                maxentry.add(up->toJSON());
            }
        }
        ret.put("maxentries", maxentry);
    }

    auto root = getRoot();
    if (!root) {
        std::any nullv;
        ret.put("root", nullv);
    } else {
        ret.put("root", root->toJSON());
    }

    return ret;
}

void MMR::finalizeSet() {
    // Reset
    mFinalized = false;

    // Save final peaks
    mFinalizedPeaks.clear();
    auto peaks = getPeaks();
    mFinalizedPeaks.reserve(peaks.size());
    for (const auto& e : peaks) {
        mFinalizedPeaks.emplace_back(std::make_unique<MMREntry>(e));
    }

    // Save final root
    mFinalizedRoot = getRoot();

    // Now finalized
    mFinalized = true;
}

void MMR::setFinalized(bool zFinalized) { mFinalized = zFinalized; }

bool MMR::isFinalized() const { return mFinalized; }

void MMR::setBlockTime(const MiniNumber& zTime) { mBlockTime = zTime; }

MiniNumber MMR::getBlockTime() const { return mBlockTime; }

const MMREntryNumber& MMR::getEntryNumber() const {
    return *mEntryNumber;
}

void MMR::clearParent() {
    mParent = nullptr;
    mUseMegaMMR = GeneralParams::IS_MEGAMMR;
}

void MMR::setParent(MMR* zMMR) {
    mParent = zMMR;
    mUseMegaMMR = GeneralParams::IS_MEGAMMR;
}

MMR* MMR::getParent() const { return mParent; }

int MMR::getTotalEntries() const { return static_cast<int>(mSetEntries.size()); }

const std::unordered_map<std::string, std::unique_ptr<MMREntry>>& MMR::getAllEntries() const {
    return mSetEntries;
}

std::string MMR::getHashTableEntry(int zRow, const MMREntryNumber& zEntry) const {
    return std::to_string(zRow) + ":" + zEntry.toString();
}

void MMR::addHashTableEntry(const MMREntry& zEntry) {
    std::string name = getHashTableEntry(zEntry.getRow(), zEntry.getEntryNumber());
    mSetEntries[name] = std::make_unique<MMREntry>(zEntry);
}

void MMR::removeHashTableEntry(const MMREntry& zEntry) {
    std::string name = getHashTableEntry(zEntry.getRow(), zEntry.getEntryNumber());
    mSetEntries.erase(name);
}

MMREntry MMR::setEntry(int zRow, const MMREntryNumber& zEntry, const MMRData& zData) {
    if (mFinalized) {
        MinimaLogger::log("SETTING IN FINALIZED MMR!");
        // Mirror Java: return a constructed entry, but do not mutate finalized state
        return MMREntry(zRow, zEntry, zData);
    }

    if (zRow > mMaxRow) {
        mMaxRow = zRow;
    }

    MMREntry entry(zRow, zEntry, zData);

    // Overwrite old one if exists
    std::string key = getHashTableEntry(zRow, zEntry);
    mSetEntries[key] = std::make_unique<MMREntry>(entry);

    // Is it a MAX
    if (!mMaxEntries[zRow]) {
        mMaxEntries[zRow] = std::make_unique<MMREntry>(entry);
    } else if (mMaxEntries[zRow]->getEntryNumber().isLessEqual(zEntry)) {
        mMaxEntries[zRow] = std::make_unique<MMREntry>(entry);
    }

    return entry;
}

MMREntry MMR::getEntry(int zRow, const MMREntryNumber& zEntry) {
    return getEntry(zRow, zEntry, MiniNumber::ZERO());
}

MMREntry MMR::getEntry(int zRow, const MMREntryNumber& zEntry, const MiniNumber& zMaxBack) const {
    const MMR* current = this;
    std::string entryname = getHashTableEntry(zRow, zEntry);

    bool MEGACHECK = false;
    while (current != nullptr || MEGACHECK) {
        if (current && current->getBlockTime().isLess(zMaxBack)) {
            break;
        }

        if (current) {
            auto it = current->mSetEntries.find(entryname);
            if (it != current->mSetEntries.end()) {
                return *(it->second);
            }
        }

        if (!MEGACHECK) {
            current = current ? current->getParent() : nullptr;

            if (current == nullptr && mUseMegaMMR) {
                MEGACHECK = true;
                //
                // FIX 1: Changed '&&' check to just 'if'
                //
                if (MinimaDB::getDB()) {
                    //
                    // FIX 2: Changed '->getMegaMMR()->' to '.getMegaMMR().'
                    //
                    current = &MinimaDB::getDB()->getMegaMMR().getMMR();
                } else {
                    current = nullptr;
                }
            }
        } else {
            break;
        }
    }

    // If you can't find it - return empty entry..
    MMREntry empty(zRow, zEntry);
    return empty;
}

MMREntry MMR::addEntry(const MMRData& zData) {
// ... (rest of file is unchanged) ...
    // Create a new leaf entry
    MMREntry entry = setEntry(0, *mEntryNumber, zData);
    MMREntry ret = entry; // keep a copy to return

    // 1 more entry
    // Re-assign the unique_ptr to a new object holding the incremented value
    mEntryNumber = std::make_unique<MMREntryNumber>(mEntryNumber->increment());

    // Now go up the tree...
    while (entry.isRight()) {
        // Get the Sibling.. will be the left
        MMREntry sibling = getEntry(entry.getRow(), entry.getSibling());

        // Create the new row - hash LEFT + RIGHT
        std::unique_ptr<MMRData> parentdata = MMRData::CreateMMRDataParentNode(*sibling.getMMRData(), *entry.getMMRData()); 

        // Set the Parent Entry
        entry = setEntry(entry.getParentRow(), entry.getParentEntry(), *parentdata); 
    }

    return ret;
}

void MMR::updateEntry(const MMREntryNumber& zEntry, const MMRProof& zOldProof, const MMRData& zNewData) {
    // Get the current peaks
    std::vector<MMREntry> peaks = getPeaks();

    // Proof time
    int currentproof = 0;
    MiniNumber prooftime = zOldProof.getBlockTime();

    // Set the Entry..
    MMREntry entry = setEntry(0, zEntry, zNewData);

    // If this is not a Peak, propagate upwards
    std::unique_ptr<MMRData> parentdata; 
    while (!entry.checkPosition(peaks)) { 
        // Get the sibling.. only go as far back as the proof
        MMREntry sibling = getEntry(entry.getRow(), entry.getSibling(), prooftime);

        // If we don't have it use the proof value..
        const MMRData* siblingdata_ptr; 
        std::unique_ptr<MMRData> siblingdata_owned; 
        if (sibling.isEmpty()) {
            const MMRData& proof_data = zOldProof.getProofChunk(currentproof).getMMRData(); 
            siblingdata_owned = std::make_unique<MMRData>(proof_data.getData(), proof_data.getValue()); 
            siblingdata_ptr = siblingdata_owned.get(); 
        } else {
            siblingdata_ptr = sibling.getMMRData(); 
        }
        currentproof++;

        // Ensure sibling exists at top level in this MMR
        MMREntry siblingtop = setEntry(sibling.getRow(), sibling.getEntryNumber(), *siblingdata_ptr); 

        // Calculate the parent
        if (entry.isLeft()) {
            parentdata = MMRData::CreateMMRDataParentNode(*entry.getMMRData(), *siblingtop.getMMRData()); 
        } else {
            parentdata = MMRData::CreateMMRDataParentNode(*siblingtop.getMMRData(), *entry.getMMRData()); 
        }

        // Set parent
        entry = setEntry(entry.getParentRow(), entry.getParentEntry(), *parentdata); 
    }
}

MMRProof MMR::getProof(const MMREntryNumber& zEntryNumber) {
    // Get this entry
    MMREntry entry = getEntry(0, zEntryNumber);

    // Basic proof to peak
    MMRProof proof = getProofToPeak(zEntryNumber);

    // Calculate the peak
    std::unique_ptr<MMRData> peak = proof.calculateProof(*entry.getMMRData()); 

    // Path from peak to root
    MMRProof rootproof = getPeakToRoot(*peak); 

    // Append
    int len = rootproof.getProofLength();
    for (int i = 0; i < len; ++i) {
        proof.addProofChunk(rootproof.getProofChunk(i));
    }

    return proof;
}

MMRProof MMR::getProofToPeak(const MMREntryNumber& zEntryNumber) {
    // First get the initial Entry.. check parents as well..
    MMREntry entry = getEntry(0, zEntryNumber);

    // Now get all the hashes in the tree to a peak..
    MMRProof proof(mBlockTime);

    // Go up to the MMR Peak..
    MMREntry sibling = getEntry(entry.getRow(), entry.getSibling());
    while (!sibling.isEmpty()) {
        // Add to proof
        proof.addProofChunk(sibling.isLeft(), *sibling.getMMRData()); 

        // Parent reference (may be empty)
        MMREntry parent(sibling.getParentRow(), sibling.getParentEntry());

        // Sibling of the parent
        sibling = getEntry(parent.getRow(), parent.getSibling());
    }

    return proof;
}

MMRProof MMR::getPeakToRoot(const MMRData& zPeak) {
    // Sum of all the initial proofs...
    MMRProof totalproof(getBlockTime());

    // Get peaks
    std::vector<MMREntry> peaks = getPeaks();

    // Now, iteratively combine peaks into a new MMR until one remains
    std::unique_ptr<MMRData> currentpeak = std::make_unique<MMRData>(zPeak.getData(), zPeak.getValue()); 
    std::optional<MMREntry> keeper; 

    while (peaks.size() > 1) {
        MMR newmmr;

        // Add all peaks to the new MMR
        for (const MMREntry& peak : peaks) {
            MMREntry current = newmmr.addEntry(*peak.getMMRData()); 
            if (peak.getMMRData()->getData().isEqual(currentpeak->getData())) { 
                keeper = current; 
            }
        }

        // Proof for this keeper
        MMRProof proof = newmmr.getProofToPeak(keeper->getEntryNumber()); 

        // Add to total proof
        int len = proof.getProofLength();
        for (int i = 0; i < len; ++i) {
            totalproof.addProofChunk(proof.getProofChunk(i));
        }

        // Recalculate: Start Peak + FULL Proof
        currentpeak = totalproof.calculateProof(zPeak); 

        // Now get the peaks of new MMR.. repeat
        peaks = newmmr.getPeaks();
    }

    return totalproof;
}

bool MMR::checkProof(const MMRData& zMMRData, const MMRProof& zProof) const {
    // Calculate the final data unit
    std::unique_ptr<MMRData> rootcalc = zProof.calculateProof(zMMRData); 

    // Check against root
    auto root = getRoot();
    if (root && rootcalc->isEqual(*root)) { 
        return true;
    }

    // Check against all peaks
    std::vector<MMREntry> peaks = getPeaks();
    for (const MMREntry& peak : peaks) {
        if (rootcalc->isEqual(*peak.getMMRData())) { 
            return true;
        }
    }

    return false;
}

bool MMR::checkProofTimeValid(const MMREntryNumber& zEntry, const MMRData& zMMRData, const MMRProof& zProof) const {
    // Parent at proof time
    const MMR* mmr = getParentAtTime(zProof.getBlockTime());
    if (mmr == nullptr) {
        return false;
    }

    // Check at that time
    if (!mmr->checkProof(zMMRData, zProof)) { 
        return false;
    }

    // Ensure no later different value
    MMREntry checker = getEntry(0, zEntry, zProof.getBlockTime());

    if (checker.isEmpty()) {
        return true;
    }

    return checker.getMMRData()->isEqual(zMMRData); 
}

std::vector<MMREntry> MMR::getPeaks() const {
    if (mFinalized) {
        // Return copies of finalized peaks
        std::vector<MMREntry> out;
        out.reserve(mFinalizedPeaks.size());
        for (const auto& p : mFinalizedPeaks) {
            out.emplace_back(*p);
        }
        return out;
    }

    std::vector<MMREntry> peaks;
    for (int i = mMaxRow; i >= 0; --i) {
        auto& maxptr = mMaxEntries[i];
        if (maxptr) {
            if (maxptr->isLeft()) {
                peaks.emplace_back(*maxptr);
            }
        }
    }

    return peaks;
}

std::unique_ptr<MMRData> MMR::getRootInternal() const {
    // Get the Peaks..
    std::vector<MMREntry> peaks = getPeaks();

    // Are there any peaks yet..
    if (peaks.empty()) {
        return nullptr;
    }

    // Now take all those values and put THEM in an MMR..
    while (peaks.size() > 1) {
        MMR newmmr;

        // Add all the peaks to it..
        for (const MMREntry& peak : peaks) {
            MMRData newpeak(peak.getMMRData()->getData(), peak.getMMRData()->getValue()); 
            newmmr.addEntry(newpeak);
        }

        // Now get the peaks.. repeat..
        peaks = newmmr.getPeaks();
    }

    return std::make_unique<MMRData>(peaks[0].getMMRData()->getData(), peaks[0].getMMRData()->getValue()); 
}

std::unique_ptr<MMRData> MMR::getRoot() const {
    if (mFinalized) {
        if (!mFinalizedRoot) {
            return nullptr;
        }
        return std::make_unique<MMRData>(mFinalizedRoot->getData(), mFinalizedRoot->getValue());
    }

    return getRootInternal();
}

const MMR* MMR::getParentAtTime(const MiniNumber& zTime) const {
    const MMR* current = this;

    while (current != nullptr) {
        if (current->getBlockTime().isEqual(zTime)) {
            return current;
        }

        if (current->getBlockTime().isLess(zTime)) {
            return nullptr;
        }

        current = current->getParent();
    }

    return nullptr;
}

void MMR::writeDataStream(std::ostream& out) {
    // Write the Block Time.
    mBlockTime.writeDataStream(out);

    // EntryNumber..
    if (mEntryNumber) {
        mEntryNumber->writeDataStream(out);
    } else {
        MMREntryNumber zero(boost::multiprecision::cpp_int(0));
        zero.writeDataStream(out);
    }

    // How many..
    MiniNumber elen(static_cast<int>(mSetEntries.size()));
    elen.writeDataStream(out);

    // Now write out each entry..
    for (const auto& kv : mSetEntries) {
        kv.second->writeDataStream(out);
    }
}

void MMR::readDataStream(std::istream& in) {
    mBlockTime = MiniNumber::ReadFromStream(in);
    auto ent = MMREntryNumber::ReadFromStream(in);
    mEntryNumber = std::make_unique<MMREntryNumber>(ent);

    // Now the Entries..
    mSetEntries.clear();
    mMaxEntries.clear();
    mMaxEntries.resize(MAXROWS);
    mMaxRow = 0;

    int len = MiniNumber::ReadFromStream(in).getAsInt();
    for (int i = 0; i < len; ++i) {
        MMREntry entry = MMREntry::ReadFromStream(in);
        setEntry(entry.getRow(), entry.getEntryNumber(), *entry.getMMRData()); 
    }

    // Finalize
    finalizeSet();
}

std::unique_ptr<MMR> MMR::ReadFromStream(std::istream& in) {
    auto mmr = std::make_unique<MMR>();
    mmr->readDataStream(in);
    return mmr;
}

void MMR::pruneTree() {
    // Get the Peaks..
    std::vector<MMREntry> peaks = getPeaks();
    for (const MMREntry& peak : peaks) {
        prune(peak);
    }
}

void MMR::prune(const MMREntry& zStartNode) {
    if (zStartNode.isEmpty()) {
        return;
    }

    int childrow = zStartNode.getChildRow();
    if (childrow < 0) {
        // Leaf nodes
        return;
    }

    // The children..
    MMREntry leftchild = getEntry(childrow, zStartNode.getLeftChildEntry());
    MMREntry rightchild = getEntry(childrow, zStartNode.getRightChildEntry());

    // Prune the children if they exist
    prune(leftchild);
    prune(rightchild);

    // Is this a ZERO node.. if so remove the children
    if (zStartNode.getMMRData()->getValue().isEqual(MiniNumber::ZERO())) { 
        removeHashTableEntry(leftchild);
        removeHashTableEntry(rightchild);
    }
}

const std::unordered_set<std::string>& MMR::getPrunedUnspendableCoins() const {
    return mPrunedCoins;
}

void MMR::scanUnspendableTree() {
    // Clear the pruned coins for a fresh start
    mPrunedCoins.clear();

    // Get the Peaks..
    std::vector<MMREntry> peaks = getPeaks();
    for (const MMREntry& peak : peaks) {
        scanUnspendable(peak);
    }
}

bool MMR::scanUnspendable(const MMREntry& zStartNode) {
    if (zStartNode.isEmpty()) {
        return true;
    }

    int childrow = zStartNode.getChildRow();
    if (childrow < 0) {
        // Leaf nodes
        return zStartNode.getMMRData()->isUnspendable(); 
    }

    // The children..
    MMREntry leftchild = getEntry(childrow, zStartNode.getLeftChildEntry());
    MMREntry rightchild = getEntry(childrow, zStartNode.getRightChildEntry());

    // Recurse
    bool leftunspend = scanUnspendable(leftchild);
    bool rightunspend = scanUnspendable(rightchild);

    bool leftzero = true;
    if (!leftchild.isEmpty()) {
        leftzero = leftchild.getMMRData()->getValue().isEqual(MiniNumber::ZERO()); 
    }

    bool rightzero = true;
    if (!rightchild.isEmpty()) {
        rightzero = rightchild.getMMRData()->getValue().isEqual(MiniNumber::ZERO()); 
    }

    // If both subtrees are zero or unspendable..
    if ((leftunspend || leftzero) && (rightunspend || rightzero)) {
        // This node is unspendable - set on the stored node if it exists
        std::string key = getHashTableEntry(zStartNode.getRow(), zStartNode.getEntryNumber());
        auto it = mSetEntries.find(key);
        if (it != mSetEntries.end()) {
            it->second->getMMRData()->setUnspendable(true); 
        }

        // Remove children
        removeHashTableEntry(leftchild);
        removeHashTableEntry(rightchild);

        // Add to pruned list if these are on row 0
        if (childrow == 0) {
            if (!leftchild.isEmpty()) {
                mPrunedCoins.insert(leftchild.getEntryNumber().toString());
            }
            if (!rightchild.isEmpty()) {
                mPrunedCoins.insert(rightchild.getEntryNumber().toString());
            }
        }

        return true;
    }

    // This node is spendable
    {
        std::string key = getHashTableEntry(zStartNode.getRow(), zStartNode.getEntryNumber());
        auto it = mSetEntries.find(key);
        if (it != mSetEntries.end()) {
            it->second->getMMRData()->setUnspendable(false); 
        }
    }

    return false;
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org
