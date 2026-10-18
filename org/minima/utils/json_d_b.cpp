#include "org/minima/utils/json_d_b.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <any>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

namespace org {
namespace minima {
namespace utils {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::parser::JSONParser;
using org::minima::utils::json::parser::ParseException;

JsonDB::JsonDB()
    : mParams(std::make_unique<JSONObject>()) {}

JsonDB::~JsonDB() = default;
JsonDB::JsonDB(JsonDB&&) noexcept = default;
JsonDB& JsonDB::operator=(JsonDB&&) noexcept = default;

JSONObject& JsonDB::getAllData() {
    return *mParams;
}

const JSONObject& JsonDB::getAllData() const {
    return *mParams;
}

bool JsonDB::exists(const std::string& zName) const {
    return mParams->containsKey(zName);
}

bool JsonDB::getBoolean(const std::string& zName, bool zDefault) const {
    if (!mParams->containsKey(zName)) {
        return zDefault;
    }
    try {
        return mParams->getBoolean(zName);
    } catch (...) {
        return zDefault;
    }
}

void JsonDB::setBoolean(const std::string& zName, bool zData) {
    mParams->put(zName, zData);
}

MiniNumber JsonDB::getNumber(const std::string& zName, const MiniNumber& zDefault) const {
    if (!mParams->containsKey(zName)) {
        return zDefault;
    }
    try {
        std::string number = mParams->getString(zName);
        
        // FIX: Check for empty string before constructing MiniNumber
        if (number.empty()) {
            return zDefault;
        }
        
        return MiniNumber(number);
    } catch (...) {
        return zDefault;
    }
}


void JsonDB::setNumber(const std::string& zName, const MiniNumber& zNumber) {
    mParams->put(zName, zNumber.toString());
}

MiniData JsonDB::getData(const std::string& zName, const MiniData& zDefault) const {
    if (!mParams->containsKey(zName)) {
        return zDefault;
    }
    try {
        std::string data = mParams->getString(zName);
        
        // FIX: Check for empty string
        if (data.empty()) {
            return zDefault;
        }
        
        return MiniData(data);
    } catch (...) {
        return zDefault;
    }
}


void JsonDB::setData(const std::string& zName, const MiniData& zData) {
    mParams->put(zName, zData.toString());
}

std::string JsonDB::getString(const std::string& zName) const {
    if (!mParams->containsKey(zName)) {
        return std::string(); // mimic null by returning empty string
    }
    try {
        return mParams->getString(zName);
    } catch (...) {
        return std::string();
    }
}

std::string JsonDB::getString(const std::string& zName, const std::string& zDefault) const {
    if (!mParams->containsKey(zName)) {
        return zDefault;
    }
    try {
        return mParams->getString(zName);
    } catch (...) {
        return zDefault;
    }
}

void JsonDB::setString(const std::string& zName, const std::string& zData) {
    mParams->put(zName, zData);
}

void JsonDB::setJSON(const std::string& zName, const JSONObject& zJSON) {
    mParams->put(zName, zJSON);
}

JSONObject JsonDB::getJSON(const std::string& zName, const JSONObject& zDefault) const {
    if (!mParams->containsKey(zName)) {
        return zDefault;
    }
    try {
        const std::any& v = mParams->get(zName);
        if (v.type() == typeid(JSONObject)) {
            return std::any_cast<JSONObject>(v);
        }
        if (v.type() == typeid(std::shared_ptr<JSONObject>)) {
            auto ptr = std::any_cast<std::shared_ptr<JSONObject>>(v);
            if (ptr) {
                return *ptr;
            }
        }
    } catch (...) {
    }
    return zDefault;
}

void JsonDB::setJSONArray(const std::string& zName, const JSONArray& zJSONArray) {
    mParams->put(zName, zJSONArray);
}

JSONArray JsonDB::getJSONArray(const std::string& zName) {
    if (!mParams->containsKey(zName)) {
        mParams->put(zName, JSONArray());
    }
    try {
        const std::any& v = mParams->get(zName);
        if (v.type() == typeid(JSONArray)) {
            return std::any_cast<JSONArray>(v);
        }
        if (v.type() == typeid(std::shared_ptr<JSONArray>)) {
            auto ptr = std::any_cast<std::shared_ptr<JSONArray>>(v);
            if (ptr) {
                return *ptr;
            }
        }
    } catch (...) {
        // fall through
    }
    JSONArray empty;
    mParams->put(zName, empty);
    return empty;
}

void JsonDB::loadDB(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        mParams = std::make_unique<JSONObject>();
        return;
    }
    try {
        // Check file size
        in.seekg(0, std::ios::end);
        std::streamsize fileSize = in.tellg();
        in.seekg(0, std::ios::beg);

        if (fileSize == 0) {
            std::cerr << "[INFO] JsonDB: Empty file, using empty JSONObject" << std::endl;
            mParams = std::make_unique<JSONObject>();
            return;
        }
        
        readDataStream(in);
    } catch (const std::exception& e) {
        std::cerr << "JsonDB loadDB readDataStream error: " << e.what() << std::endl;
        mParams = std::make_unique<JSONObject>();
    } catch (...) {
        std::cerr << "JsonDB loadDB readDataStream unknown error" << std::endl;
        mParams = std::make_unique<JSONObject>();
    }
}

void JsonDB::saveDB(const std::string& filePath) {
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "JsonDB saveDB: cannot open file for writing: " << filePath << std::endl;
        return;
    }
    writeDataStream(out);
    out.flush();  
    out.close();  
}

void JsonDB::writeDataStream(std::ostream& out) {
    MiniString data(mParams->toString());
    data.writeDataStream(out);
}

void JsonDB::readDataStream(std::istream& in) {
    try {
        // Check if stream is at EOF before trying to read
        if (in.peek() == std::istream::traits_type::eof()) {
            std::cerr << "[INFO] JsonDB: Empty stream, initializing empty JSONObject" 
                      << std::endl;
            mParams = std::make_unique<JSONObject>();
            return;
        }

        MiniString data = MiniString::ReadFromStream(in);
        // DEBUG
        // std::string json_str = data.toString();
        
        // std::cerr << "[DEBUG JsonDB::READ] JSON length: " << json_str.size()
        //           << ", first 100 chars: "
        //           << json_str.substr(0, std::min(size_t(100), json_str.size()))
        //           << std::endl;
        // DEBUG END

        JSONParser parser;
        std::any parsed = parser.parse(data.toString());

        if (parsed.type() == typeid(JSONObject)) {
            mParams = std::make_unique<JSONObject>(std::any_cast<JSONObject>(parsed));
        } else if (parsed.type() == typeid(std::shared_ptr<JSONObject>)) {
            auto pobj = std::any_cast<std::shared_ptr<JSONObject>>(parsed);
            if (pobj) {
                mParams = std::make_unique<JSONObject>(*pobj);
            } else {
                mParams = std::make_unique<JSONObject>();
            }
        } else {
            mParams = std::make_unique<JSONObject>();
        }
    } catch (const std::ios_base::failure& e) {
        // Handle EOF/stream errors gracefully
        std::cerr << "[INFO] JsonDB: Empty file (EOF), initializing empty JSONObject" << std::endl;
        mParams = std::make_unique<JSONObject>();
    } catch (const ParseException& e) {
        std::cerr << "[ERROR JsonDB::READ] Parse exception: " << e.getMessage() << std::endl;
        mParams = std::make_unique<JSONObject>();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR JsonDB::READ] JsonDB parse std::exception: " << e.what() << std::endl;
        mParams = std::make_unique<JSONObject>();
    } catch (...) {
        std::cerr << "[ERROR JsonDB::READ] JsonDB parse unknown error" << std::endl;
        mParams = std::make_unique<JSONObject>();
    }
}

} // namespace utils
} // namespace minima
} // namespace org