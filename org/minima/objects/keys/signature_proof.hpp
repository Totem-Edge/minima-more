#pragma once

#include <memory>
#include <string>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Forward declarations for project dependencies (Pitfall 10)
namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
    class MiniNumber;
} } } }

namespace org { namespace minima { namespace objects { namespace mmr {
    class MMRData;
    class MMRProof;
} } } }

namespace org {
namespace minima {
namespace objects {
namespace keys {

class SignatureProof : public org::minima::utils::Streamable {
public:
    // Constructor matching Java API
    SignatureProof(const org::minima::objects::base::MiniData& zPublicKey,
                   const org::minima::objects::base::MiniData& zSignature,
                   const org::minima::objects::mmr::MMRProof& zProof);

    // Special members for unique_ptr to forward-declared types (PIMPL fix)
    virtual ~SignatureProof();
    SignatureProof(SignatureProof&&) noexcept;
    SignatureProof& operator=(SignatureProof&&) noexcept;

    // Delete copy operations to avoid slicing and ensure unique ownership
    SignatureProof(const SignatureProof& zOther);
    SignatureProof& operator=(const SignatureProof&) = delete;

    // Accessors (Java returns object references; here const references)
    const org::minima::objects::base::MiniData& getPublicKey() const;
    const org::minima::objects::base::MiniData& getSignature() const;
    const org::minima::objects::mmr::MMRProof& getProof() const;

    // Compute and return the root public key (copy, as the source is temporary)
    org::minima::objects::base::MiniData getRootPublicKey() const;

    // JSON representation
    org::minima::utils::json::JSONObject toJSON() const;

    // Streamable interface
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static read helper (Java-style)
    static std::unique_ptr<SignatureProof> ReadFromStream(std::istream& in);

private:
    // Private default constructor to mirror Java's private no-arg constructor
    SignatureProof();

    std::unique_ptr<org::minima::objects::base::MiniData> mPublicKey;
    std::unique_ptr<org::minima::objects::base::MiniData> mSignature;
    std::unique_ptr<org::minima::objects::mmr::MMRProof> mProof;
};

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org