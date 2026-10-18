#pragma once

#include <memory>
#include <vector>
#include <ostream>
#include <istream>

#include "org/minima/utils/streamable.hpp"

// Namespaced forward declarations
namespace org { namespace minima { namespace objects { namespace keys { class SignatureProof; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {
namespace keys {

class Signature : public org::minima::utils::Streamable {
public:
    Signature();
    virtual ~Signature();
    Signature(Signature&&) noexcept;
    Signature& operator=(Signature&&) noexcept;

    Signature(const Signature& zOther);
    Signature& operator=(const Signature&) = delete;

    // Add a signature proof (takes ownership)
    void addSignatureProof(std::unique_ptr<org::minima::objects::keys::SignatureProof> zSignature);

    // Access all signature proofs
    const std::vector<std::unique_ptr<org::minima::objects::keys::SignatureProof>>& getAllSignatureProofs() const;

    // Get the root public key from the first signature proof
    org::minima::objects::base::MiniData getRootPublicKey() const;

    // JSON serialization
    org::minima::utils::json::JSONObject toJSON() const;

    // Streamable interface (std::ostream/std::istream)
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helper matching Java API
    static std::unique_ptr<Signature> ReadFromStream(std::istream& in);

    // Convert a MiniData version into a Signature (returns nullptr on error)
    static std::unique_ptr<Signature> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

private:
    std::vector<std::unique_ptr<org::minima::objects::keys::SignatureProof>> mSignatures;
};

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org