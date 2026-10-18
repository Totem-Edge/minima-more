#pragma once

#include <memory>
#include <vector>
#include <string>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations for project dependencies
namespace org { namespace minima { namespace objects { namespace keys { class Signature; } } } }
namespace org { namespace minima { namespace objects { class CoinProof; } } }
namespace org { namespace minima { namespace objects { class ScriptProof; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {

class Witness : public org::minima::utils::Streamable {
public:
    Witness();
    virtual ~Witness();                         // For unique_ptr to incomplete types
    Witness(Witness&&) noexcept;                // Move operations explicitly declared
    Witness& operator=(Witness&&) noexcept;

    Witness(const Witness& zOther);           // No copying
    Witness& operator=(const Witness&) = delete;

    // Signature functions
    void clearSignatures();
    // Takes ownership of the signature; throws std::invalid_argument if nullptr
    void addSignature(std::unique_ptr<org::minima::objects::keys::Signature> zSigProof);

    // Getters (modifiable and const)
    std::vector<std::unique_ptr<org::minima::objects::keys::Signature>>& getAllSignatures();
    const std::vector<std::unique_ptr<org::minima::objects::keys::Signature>>& getAllSignatures() const;

    // Returns all root public keys for added signatures
    std::vector<org::minima::objects::base::MiniData> getAllSignatureKeys() const;

    // Check if already signed by given public key hex string
    bool isSignedBy(const std::string& zPublicKey) const;

    // MMR functions
    void clearCoinProofs();
    void addCoinProof(std::unique_ptr<org::minima::objects::CoinProof> zProof);

    std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs();
    const std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& getAllCoinProofs() const;

    // Script proofs
    void clearScriptProofs();
    void addScript(std::unique_ptr<org::minima::objects::ScriptProof> zScriptProof);
    org::minima::objects::ScriptProof* getScript(const org::minima::objects::base::MiniData& zAddress);
    const org::minima::objects::ScriptProof* getScript(const org::minima::objects::base::MiniData& zAddress) const;

    std::vector<std::unique_ptr<org::minima::objects::ScriptProof>>& getAllScripts();
    const std::vector<std::unique_ptr<org::minima::objects::ScriptProof>>& getAllScripts() const;

    // JSON and string
    org::minima::utils::json::JSONObject toJSON() const;
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

private:
    bool scriptExists(const org::minima::objects::base::MiniData& zAddress) const;

    std::vector<std::unique_ptr<org::minima::objects::keys::Signature>> mSignatureProofs;
    std::vector<std::unique_ptr<org::minima::objects::CoinProof>>       mCoinProofs;
    std::vector<std::unique_ptr<org::minima::objects::ScriptProof>>     mScriptProofs;
};

} // namespace objects
} // namespace minima
} // namespace org