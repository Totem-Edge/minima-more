#include "org/minima/system/commands/base/mmrproof.hpp"

#include <cctype>
#include <algorithm>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No OS-specific logic required here, placeholder for completeness.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;

// Local trim helper to match Java String.trim()
static inline std::string trim_copy(const std::string& s) {
    auto begin = s.begin();
    auto end = s.end();

    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    while (end != begin) {
        auto prev = end;
        --prev;
        if (!std::isspace(static_cast<unsigned char>(*prev))) break;
        end = prev;
    }
    return std::string(begin, end);
}

// mmrleafnode special members (PIMPL fix)
mmrproof::mmrleafnode::~mmrleafnode() = default;
mmrproof::mmrleafnode::mmrleafnode(mmrleafnode&&) noexcept = default;
mmrproof::mmrleafnode& mmrproof::mmrleafnode::operator=(mmrleafnode&&) noexcept = default;

// Constructor
mmrproof::mmrproof()
    : org::minima::system::commands::Command(
          "mmrproof",
          "[data:] [proof:] [root:] - Check an MMR proof") {
}

// Full help text
std::string mmrproof::getFullHelp() const {
    return
        "\nmmrproof\n"
        "\n"
        "Check an MMR Proof.\n"
        "\n"
        "Can be used to check MMR Proof of coins, scripts or a custom MMR tree created with the 'mmrcreate' command.\n"
        "\n"
        "Returns true if the proof is valid or false if not.\n"
        "\n"
        "data:\n"
        "    String/HEX data of an MMR leaf node.\n"
        "\n"
        "proof:\n"
        "    The MMR proof of the data from the 'mmrcreate' command.\n"
        "\n"
        "root:\n"
        "    The root hash of the MMR tree from the 'mmrcreate' command.\n"
        "\n"
        "Examples:\n"
        "\n"
        "mmrproof data:0xCD34.. proof:0xFED5.. root:0xDAE6..\n";
}

// Valid params
std::vector<std::string> mmrproof::getValidParams() const {
    return { "data", "proof", "root" };
}

// Run command
std::unique_ptr<JSONObject> mmrproof::runCommand() {
    // Prepare reply
    auto ret = getJSONReply();

    // Ensure required params exist
    if (!existsParam("root") || !existsParam("proof") || !existsParam("data")) {
        throw CommandException("MUST Specify data, root and proof");
    }

    // Get and parse 'data' param (may include ":<sumvalue>")
    std::string checkdata = getParam("data");
    MiniNumber mnum = MiniNumber::ZERO();
    std::string strdata = checkdata;

    std::size_t index = checkdata.find(':');
    if (index != std::string::npos) {
        strdata = trim_copy(checkdata.substr(0, index));
        std::string mnumstr = trim_copy(checkdata.substr(index + 1));
        mnum = MiniNumber(mnumstr);
    }

    // Get and parse 'root' param (may include ":<sumvalue>")
    std::string fullrootstr = getParam("root");
    MiniNumber rootnum = MiniNumber::ZERO();
    std::string rootstr = fullrootstr;

    index = fullrootstr.find(':');
    if (index != std::string::npos) {
        rootstr = trim_copy(fullrootstr.substr(0, index));
        std::string rnumstr = trim_copy(fullrootstr.substr(index + 1));
        rootnum = MiniNumber(rnumstr);
    }

    // Proof param
    std::string proofstr = getParam("proof");

    // Build leaf MMRData depending on whether 'strdata' is HEX (starts with "0x")
    std::unique_ptr<MMRData> mmrdata;
    if (strdata.rfind("0x", 0) == 0) {
        MiniData d(strdata);
        mmrdata = MMRData::CreateMMRDataLeafNode(d, mnum);
    } else {
        MiniString s(strdata);
        mmrdata = MMRData::CreateMMRDataLeafNode(s, mnum);
    }

    // Create root MMRData and proof
    MMRData root(MiniData(rootstr), rootnum);
    MiniData proof(proofstr);

    // Convert proof and compute final root value
    MMRProof prf = MMRProof::convertMiniDataVersion(proof);
    std::unique_ptr<MMRData> prfcalc = prf.calculateProof(*mmrdata);

    // Build response JSON
    JSONObject resp;
    resp.put("input", strdata);
    resp.put("leaf", mmrdata->toJSON());
    resp.put("finaldata", prfcalc->toJSON());
    resp.put("valid", prfcalc->isEqual(root));

    // Add to reply
    ret->put("response", resp);

    return ret;
}

// Factory method
org::minima::system::commands::Command* mmrproof::getFunction() {
    return new mmrproof();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org