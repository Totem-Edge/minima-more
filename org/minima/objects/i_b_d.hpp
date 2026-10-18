#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <ostream>
#include <istream>

#include <boost/multiprecision/cpp_int.hpp>

// Base class include (inheritance)
#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations (Pitfall 10)
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace archive { class ArchiveManager; } } } }
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; class CascadeNode; } } } }
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; class TxPowTree; } } } }
namespace org { namespace minima { namespace objects { class TxBlock; class TxPoW; class Greeting; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniByte; class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace system { class Main; } } }
namespace org { namespace minima { namespace utils { class MinimaLogger; } } }

namespace org {
namespace minima {
namespace objects {

class IBD : public org::minima::utils::Streamable {
public:
    // Constants
    static const org::minima::objects::base::MiniNumber MAX_BLOCKS_FOR_IBD;

    IBD();

    // Destructor and move operations (PIMPL-FIX due to unique_ptr<Cascade>)
    virtual ~IBD();
    IBD(IBD&&) noexcept;
    IBD& operator=(IBD&&) noexcept;

    // Delete copy
    IBD(const IBD&) = delete;
    IBD& operator=(const IBD&) = delete;

    // Accessors
    org::minima::objects::base::MiniNumber getTreeRoot() const;
    org::minima::objects::base::MiniNumber getTreeTip() const;

    // Builders
    bool createIBD(const org::minima::objects::Greeting& zGreeting);
    void createCompleteIBD();
    void createSyncIBD(const org::minima::objects::TxPoW& zLastBlock);
    void createArchiveIBD(const org::minima::objects::base::MiniNumber& zFirstBlock);
    void createArchiveIBD(const org::minima::objects::base::MiniNumber& zFirstBlock,
                          org::minima::database::archive::ArchiveManager& zArchiveDB,
                          bool zUseLocal);

    // Cascade
    // Copy semantics like Java: "This will be a copy - not the original"
    void setCascade(const org::minima::database::cascade::Cascade& zCascade);
    // Convenience setter that takes ownership directly
    void setCascade(std::unique_ptr<org::minima::database::cascade::Cascade> zCascade);
    org::minima::database::cascade::Cascade* getCascade() const;
    bool hasCascade() const;
    bool hasCascadeWithBlocks() const;

    // Blocks
    std::vector<std::shared_ptr<org::minima::objects::TxBlock>>& getTxBlocks();
    const std::vector<std::shared_ptr<org::minima::objects::TxBlock>>& getTxBlocks() const;
    void setTxBlocks(std::vector<std::shared_ptr<org::minima::objects::TxBlock>> zBlocks);

    // Validation
    bool checkValidData() const;

    // Weight
    boost::multiprecision::cpp_int getTotalWeight() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static IBD ReadFromStream(std::istream& in);

    static bool checkOurChainHeavier(const IBD& zIBD);
    static IBD createShortenedIBD(const IBD& zIBD, const std::string& zTxPOWID);
    static void printIBD(const IBD& zIBD);

private:
    // Members
    std::unique_ptr<org::minima::database::cascade::Cascade> mCascade; // nullable
    std::vector<std::shared_ptr<org::minima::objects::TxBlock>> mTxBlocks;
};

} // namespace objects
} // namespace minima
} // namespace org