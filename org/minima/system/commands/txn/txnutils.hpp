#pragma once

#include <string>
#include <vector>

// Forward declarations (PITFALL 4)
namespace org { namespace minima { namespace objects {
    class Transaction;
    class Witness;
    class Coin;
} } }

namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace database { namespace userprefs { namespace txndb {
    class TxnRow;
} } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnutils {
public:
    // Set MMR proofs and script proofs for a whole transaction
    static void setMMRandScripts(org::minima::objects::Transaction& zTransaction,
                                 org::minima::objects::Witness& zWitness);

    static void setMMRandScripts(org::minima::objects::Transaction& zTransaction,
                                 org::minima::objects::Witness& zWitness,
                                 bool zExitOnFail);

    // Set MMR proofs and script proofs for a single coin
    static void setMMRandScripts(const org::minima::objects::Coin& zCoin,
                                 org::minima::objects::Witness& zWitness);

    // Burn transaction creators
    static org::minima::database::userprefs::txndb::TxnRow
    createBurnTransaction(const std::vector<std::string>& zExcludeCoins,
                          const org::minima::objects::base::MiniData& zLinkTransactionID,
                          const org::minima::objects::base::MiniNumber& zAmount);

    static org::minima::database::userprefs::txndb::TxnRow
    createBurnTransaction(const std::vector<std::string>& zExcludeCoins,
                          const org::minima::objects::base::MiniData& zLinkTransactionID,
                          const org::minima::objects::base::MiniNumber& zAmount,
                          bool zSign);
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org