#pragma once

#include <memory>
#include <string>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations (do not include full headers here)
namespace org { namespace minima { namespace objects { class TxPoW; } } }

namespace org {
namespace minima {
namespace database {
namespace cascade {

class CascadeNode : public org::minima::utils::Streamable {
public:
    // Constructors
    CascadeNode(); // will create an empty node; primarily for ReadFromStream
    explicit CascadeNode(const org::minima::objects::TxPoW& zTxPoW);

    // Destructor and move operations (PIMPL/unique_ptr to incomplete types)
    virtual ~CascadeNode();
    CascadeNode(CascadeNode&&) noexcept;
    CascadeNode& operator=(CascadeNode&&) noexcept;

    // Delete copy
    CascadeNode(const CascadeNode&) = delete;
    CascadeNode& operator=(const CascadeNode&) = delete;

    // Accessors
    org::minima::objects::TxPoW& getTxPoW();
    const org::minima::objects::TxPoW& getTxPoW() const;

    int getLevel() const;
    void setLevel(int zLevel);

    int getSuperLevel() const;

    void setParent(CascadeNode* zCascadeNode);
    CascadeNode* getParent() const;

    // BigDecimal mapped behavior: return decimal string
    std::string getCurrentWeight() const;

    // String representation
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static factory from stream
    static std::unique_ptr<CascadeNode> ReadFromStream(std::istream& in);

private:
    // Members
    std::unique_ptr<org::minima::objects::TxPoW> mTxPoW; // header only - cleared body
    int mCurrentLevel = 0;
    int mSuperLevel   = 0;

    CascadeNode* mParent = nullptr;
};

} // namespace cascade
} // namespace database
} // namespace minima
} // namespace org