#pragma once

#include <string>
#include <stdexcept>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <boost/multiprecision/cpp_int.hpp>

namespace org {
namespace minima {
namespace objects {

class Magic : public org::minima::utils::Streamable {
public:
    // Public static constants (Java public static final)
    static const org::minima::objects::base::MiniNumber MIN_HASHES;
    static const org::minima::objects::base::MiniNumber DEFAULT_TXPOW_SIZE;
    static const org::minima::objects::base::MiniNumber DEFAULT_KISSVM_OPERATIONS;
    static const org::minima::objects::base::MiniNumber DEFAULT_TXPOW_TXNS;

    // Java BigInteger MIN_TXPOW_VAL
    static const boost::multiprecision::cpp_int MIN_TXPOW_VAL;
    // MiniData derived from MIN_TXPOW_VAL
    static const org::minima::objects::base::MiniData MIN_TXPOW_WORK;

    // Members: current and desired values
    org::minima::objects::base::MiniNumber mCurrentMaxTxPoWSize;
    org::minima::objects::base::MiniNumber mCurrentMaxKISSVMOps;
    org::minima::objects::base::MiniNumber mCurrentMaxTxnPerBlock;
    org::minima::objects::base::MiniData   mCurrentMinTxPoWWork;

    org::minima::objects::base::MiniNumber mDesiredMaxTxPoWSize;
    org::minima::objects::base::MiniNumber mDesiredMaxKISSVMOps;
    org::minima::objects::base::MiniNumber mDesiredMaxTxnPerBlock;
    org::minima::objects::base::MiniData   mDesiredMinTxPoWWork;

    // Constructor
    Magic();

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;

    // Equality check for the "current" set (matches Java checkSame)
    bool checkSame(const Magic& zMagic) const;

    // Getters (return by value, matching Java semantics)
    org::minima::objects::base::MiniNumber getMaxTxPoWSize() const;
    org::minima::objects::base::MiniNumber getMaxKISSOps() const;
    org::minima::objects::base::MiniNumber getMaxNumTxns() const;
    org::minima::objects::base::MiniData   getMinTxPowWork() const;

    // Setters for desired values
    void setDesiredKISSVM(const org::minima::objects::base::MiniNumber& zKISSVM);
    void setDesiredMaxTxPoWSize(const org::minima::objects::base::MiniNumber& zMaxSize);
    void setDesiredMaxTxns(const org::minima::objects::base::MiniNumber& zMaxTxn);

    // Calculate the new current by heavily weighted average
    Magic calculateNewCurrent() const;

    // String
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helper to read from stream
    static Magic ReadFromStream(std::istream& in);

private:
    // Weighted calculation constants and hard limits (private static)
    static const org::minima::objects::base::MiniNumber CALC_WEIGHTED; // 16383
    static const org::minima::objects::base::MiniNumber CALC_TOTAL;    // 16384

    static const org::minima::objects::base::MiniNumber MINMAX_TXPOW_SIZE;         // 64*1024
    static const org::minima::objects::base::MiniNumber MINMAX_KISSVM_OPERATIONS;  // 1024
    static const org::minima::objects::base::MiniNumber MINMAX_TXPOW_TXNS;         // 256
};

} // namespace objects
} // namespace minima
} // namespace org