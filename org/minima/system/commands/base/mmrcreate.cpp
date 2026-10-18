#include "org/minima/system/commands/base/mmrcreate.hpp"

#include <stdexcept>
#include <typeinfo>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp" // <--- ADDED THIS INCLUDE

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

mmrcreate::mmrcreate()
    : org::minima::system::commands::Command(
          "mmrcreate",
          "[nodes:[]] - Create an MMR Tree of data. Nodes can be STRING / HEX") {}

std::string mmrcreate::getFullHelp() const {
    return std::string("\nmmrcreate\n")
        + "\n"
        + "Create an MMR Tree of data. Can be used in MAST contracts.\n"
        + "\n"
        + "Must specify a JSON array of string/HEX data for the leaf nodes.\n"
        + "\n"
        + "They could be a list of public keys.. and then in a script you can check if the given key is one in the set.\n"
        + "\n"
        + "OR you can have different scripts.. and then you can execute any number of scripts from the same UTXO.\n"
        + "\n"
        + "Returns the MMR data and proof for each leaf node and the MMR root hash.\n"
        + "\n"
        + "nodes:\n"
        + "    JSON array of string/HEX data for the leaf nodes.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "mmrcreate nodes:[\"RETURN TRUE\",\"RETURN FALSE\"]\n"
        + "\n"
        + "mmrcreate nodes:[\"0xFF..\",\"0xEE..\"]\n";
}

std::vector<std::string> mmrcreate::getValidParams() const {
    return std::vector<std::string>{"nodes"};
}

std::unique_ptr<JSONObject> mmrcreate::runCommand() {
    // Get the reply shell
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the array of nodes (throws if missing/invalid)
    std::unique_ptr<JSONArray> mmrdata = getJSONArrayParam("nodes");

    // Create an MMR
    MMR mmrtree;

    // Helper leaf node struct (internal to function)
    struct mmrleafnode {
        int mEntry = 0;
        std::string mInput;
        std::unique_ptr<MMRData> mLeafData;
    };

    // Add the data
    int counter = 0;
    std::vector<mmrleafnode> leafnodes;
    leafnodes.reserve(mmrdata ? mmrdata->size() : 0);

    for (const std::any& elem : mmrdata->elements()) {
        // Expect each element to be a string
        std::string fulldata;
        if (elem.type() == typeid(std::string)) {
            fulldata = std::any_cast<std::string>(elem);
        } else if (elem.type() == typeid(const char*)) {
            fulldata = std::string(std::any_cast<const char*>(elem));
        } else if (elem.type() == typeid(char*)) {
            fulldata = std::string(std::any_cast<char*>(elem));
        } else {
            throw std::runtime_error("mmrcreate: nodes array must contain only strings");
        }

        // The data and SUM value..
        std::string strdata;
        MiniNumber mnum = MiniNumber::ZERO();

        // Is there a sum value
        std::size_t index = fulldata.find(':');
        if (index != std::string::npos) {
            strdata = fulldata.substr(0, index);
            mnum = MiniNumber(fulldata.substr(index + 1));
        } else {
            strdata = fulldata;
        }

        // Build the leaf data
        std::unique_ptr<MMRData> leaf;

        // Is it HEX
        if (strdata.rfind("0x", 0) == 0) {
            MiniData md(strdata);
            leaf = MMRData::CreateMMRDataLeafNode(md, mnum);
        } else {
            MiniString ms(strdata);
            leaf = MMRData::CreateMMRDataLeafNode(ms, mnum);
        }

        if (!leaf) {
            throw std::runtime_error("mmrcreate: failed to create MMR leaf data");
        }

        // Create leafnode
        mmrleafnode ln;
        ln.mEntry = counter;
        ln.mInput = strdata;
        ln.mLeafData = std::move(leaf);

        // Add them to our list
        leafnodes.emplace_back(std::move(ln));

        // Increment
        counter++;

        // Add to the MMR
        mmrtree.addEntry(*leafnodes.back().mLeafData);
    }

    // Get the root
    std::unique_ptr<MMRData> root = mmrtree.getRoot();
    if (!root) {
        throw std::runtime_error("mmrcreate: failed to compute MMR root");
    }

    // Now create the output
    JSONArray leafdata;
    for (const mmrleafnode& leaf : leafnodes) {
        JSONObject jobj;
        jobj.put("entry", leaf.mEntry);
        jobj.put("data", leaf.mInput);
        jobj.put("value", leaf.mLeafData->getValue().toString());
        // jobj.put("leafnode", leaf.mLeafData->toJSON());

        // Get the proof
        MMRProof proof = mmrtree.getProof(MMREntryNumber(leaf.mEntry));

        // Get as data string
        std::unique_ptr<MiniData> dataproof = MiniData::getMiniDataVersion(proof);
        if (!dataproof) {
            throw std::runtime_error("mmrcreate: failed to serialize proof to MiniData");
        }

        // Add this proof
        jobj.put("proof", dataproof->to0xString());

        // Add to the total
        leafdata.add(jobj);
    }

    JSONObject jsonmmr;
    jsonmmr.put("nodes", leafdata);
    jsonmmr.put("total", static_cast<int>(leafnodes.size()));
    jsonmmr.put("root", root->toJSON());

    // Add response
    ret->put("response", jsonmmr);

    return ret;
}

org::minima::system::commands::Command* mmrcreate::getFunction() {
    return new mmrcreate();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
