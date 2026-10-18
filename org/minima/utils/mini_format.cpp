#include "org/minima/utils/mini_format.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <vector>
#include <any>
#include <stdexcept>
#include <iostream>
#include <cctype>

#include <boost/multiprecision/cpp_int.hpp>

namespace org {
namespace minima {
namespace utils {

using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::parser::JSONParser;
using org::minima::utils::json::parser::ParseException;

static void MiniFormat_DebugLog(const std::string& msg) {
    std::cerr << msg << std::endl;
}

JSONArray MiniFormat::convertToJSON(const std::string& zJsonString) {
    JSONParser parser;
    std::any parsed = parser.parse(zJsonString);
    // Expect an array; mirror Java cast behavior
    try {
        return anyToJSONArray(parsed);
    } catch (const std::bad_any_cast&) {
        // In Java this would be a ClassCastException (unchecked). Here we throw a ParseException.
        throw ParseException(ParseException::ERROR_UNEXPECTED_TOKEN, std::string("Expected JSONArray"));
    } catch (const std::exception& e) {
        throw ParseException(ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string(e.what()));
    }
}

std::string MiniFormat::JSONPretty(const JSONArray& zJSONArray) {
    return JSONPretty(zJSONArray, false);
}

static const size_t MAX_PRETTY_PRINT_SIZE = 50000;

std::string MiniFormat::JSONPretty(const JSONArray& zJSONArray, bool zDebug) {
    if (zJSONArray.size() > MAX_PRETTY_PRINT_SIZE) {
        // Create a JSON error object
        JSONObject err;
        err.put("status", false);
        err.put("message", "Response is too large to pretty-print.");
        err.put("elements", std::to_string(zJSONArray.size()));
        
        // Put it in an array and return the compact string
        JSONArray err_arr;
        err_arr.add(err);
        return err_arr.toJSONString(); // This is memory-safe
    }
    
    bool arr = false;
    if (zJSONArray.size() > 1) {
        arr = true;
    }

    std::string result;
    if (arr) {
        result = "[";
    }

    bool first = true;
    const auto& elems = zJSONArray.elements();
    for (const auto& elem : elems) {
        if (!first) {
            result += ",";
        }
        first = false;

        // Expect JSONObject; mirror Java's unchecked behavior
        JSONObject jobj = anyToJSONObject(elem);
        std::string res = JSONPretty(jobj, zDebug);
        result += res;
    }

    if (arr) {
        result += "]";
    }

    return result;
}

std::string MiniFormat::JSONPretty(const JSONObject& zJSONObj) {
    return JSONPretty(zJSONObj, false);
}

std::string MiniFormat::JSONPretty(const JSONObject& zJSONObj, bool zDebug) {
    // Work string
    std::string work = trim(zJSONObj.toString());

    int len = static_cast<int>(work.size());

    if (zDebug) {
        MiniFormat_DebugLog(std::string("JSONPRETTY len:") + std::to_string(len) + " obj:" + zJSONObj.toString());
    }

    std::string ret;

    try {
        int tabs = 0;
        std::string tabstring = maketabstring(tabs);
        std::size_t oldpos = 0;
        std::size_t currentpos = 0;

        while (true) {
            oldpos = currentpos;

            std::size_t indquotes = work.find('"', currentpos);
            std::size_t indopen   = work.find('{', currentpos);
            std::size_t indclose  = work.find('}', currentpos);
            std::size_t indcomma  = work.find(',', currentpos);

            if (indquotes == std::string::npos &&
                indopen   == std::string::npos &&
                indclose  == std::string::npos &&
                indcomma  == std::string::npos) {
                // Add the rest
                ret += work.substr(currentpos, work.size() - currentpos);
                break;
            }

            if (indquotes == std::string::npos) indquotes = std::string::npos - 1;
            if (indopen   == std::string::npos) indopen   = std::string::npos - 1;
            if (indclose  == std::string::npos) indclose  = std::string::npos - 1;
            if (indcomma  == std::string::npos) indcomma  = std::string::npos - 1;

            if (indopen < indclose && indopen < indcomma && indopen < indquotes) {
                // OPEN BRACE
                tabs++;
                tabstring = maketabstring(tabs);

                std::string substr = work.substr(oldpos, indopen - oldpos);
                currentpos = indopen + 1;
                ret += substr;

                ret += "{\n";
                ret += tabstring;
            } else if (indclose < indopen && indclose < indcomma && indclose < indquotes) {
                // CLOSE BRACE
                tabs--;
                if (tabs < 0) tabs = 0;
                tabstring = maketabstring(tabs);

                std::string substr = work.substr(oldpos, indclose - oldpos);
                currentpos = indclose + 1;
                ret += substr;

                ret += "\n";
                ret += tabstring;
                ret += "}";
            } else if (indquotes < indopen && indquotes < indcomma && indquotes < indclose) {
                // Quoted string
                std::string prequote = work.substr(oldpos, indquotes - oldpos);
                ret += prequote;

                std::size_t quoteend = work.find('"', indquotes + 1);

                // Extract text (naively; no escape handling as per original)
                std::string quote = work.substr(indquotes, (quoteend - indquotes) + 1);

                currentpos = quoteend + 1;
                ret += quote;
            } else {
                // COMMA
                std::string substr = work.substr(oldpos, indcomma - oldpos);
                currentpos = indcomma + 1;
                ret += substr;

                ret += ",\n";
                ret += tabstring;
            }
        }

        // Clean up the response
        replaceAllInPlace(ret, "\\/", "/");
        replaceAllInPlace(ret, "\\n", "\n");
    } catch (const std::exception&) {
        if (zDebug) {
            MiniFormat_DebugLog(std::string("[!] Error JSONPretty.. return unmodified : ") + work);
        }
        return work;
    } catch (...) {
        if (zDebug) {
            MiniFormat_DebugLog(std::string("[!] Error JSONPretty.. return unmodified : ") + work);
        }
        return work;
    }

    return ret;
}

std::string MiniFormat::maketabstring(int zNum) {
    std::string ret;
    ret.reserve(static_cast<std::size_t>(zNum) * 2);
    for (int i = 0; i < zNum; ++i) {
        ret += "  ";
    }
    return ret;
}

std::string MiniFormat::formatSize(long long v) {
    if (v < 1024) {
        return std::to_string(v) + " bytes";
    }
    std::uint64_t uv = static_cast<std::uint64_t>(v);
    int lz = numberOfLeadingZeros64(uv);
    int z = (63 - lz) / 10;
    double val = static_cast<double>(uv) / static_cast<double>(1ULL << (z * 10));
    static const char* units = " KMGTPE";
    char unit = units[z];

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(1) << val << " " << unit << "B";
    return oss.str();
}

std::string MiniFormat::zeroPad(int zTotLength, const org::minima::objects::base::MiniNumber& zNumber) {
    std::string num = zNumber.floor().toString();
    int len = static_cast<int>(num.size());
    int add = zTotLength - len;
    for (int i = 0; i < add; ++i) {
        num = "0" + num;
    }
    return num;
}

std::string MiniFormat::ConvertMilliToTime(long long zMilli) {
    long long milliseconds = zMilli;

    long long day_ms = 24LL * 60LL * 60LL * 1000LL;
    long long hour_ms = 60LL * 60LL * 1000LL;
    long long minute_ms = 60LL * 1000LL;
    long long second_ms = 1000LL;

    long long dy = milliseconds / day_ms;

    long long yr = dy / 365;
    dy %= 365;

    long long mn = dy / 30;
    dy %= 30;

    long long wk = dy / 7;
    dy %= 7;

    long long hr = (milliseconds / hour_ms) - ((milliseconds / day_ms) * 24LL);
    long long min = (milliseconds / minute_ms) - ((milliseconds / hour_ms) * 60LL);
    long long sec = (milliseconds / second_ms) - ((milliseconds / minute_ms) * 60LL);
    long long ms  = (milliseconds) - ((milliseconds / second_ms) * second_ms);
    (void)ms; // computed but not used, matching Java's behavior

    std::ostringstream oss;
    oss << yr << " Years " << mn << " Months " << wk << " Weeks " << dy
        << " Days " << hr << " Hours " << min << " Minutes " << sec << " Seconds";
    return oss.str();
}

std::string MiniFormat::createRandomString(int len) {
    if (len <= 0) {
        return std::string("0");
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(len));
    // Java's Random is PRNG; seed with random_device for variability
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& b : data) {
        b = static_cast<std::uint8_t>(dist(gen));
    }

    return toBase32Upper(data);
}

std::string MiniFormat::trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

void MiniFormat::replaceAllInPlace(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

int MiniFormat::numberOfLeadingZeros64(std::uint64_t x) {
    if (x == 0) return 64;
    int n = 0;
    std::uint64_t mask = (1ULL << 63);
    while ((x & mask) == 0) {
        ++n;
        mask >>= 1;
    }
    return n;
}

JSONObject MiniFormat::anyToJSONObject(const std::any& a) {
    // Try direct value
    if (a.type() == typeid(JSONObject)) {
        return std::any_cast<JSONObject>(a);
    }
    // Try pointer
    if (a.type() == typeid(JSONObject*)) {
        JSONObject* p = std::any_cast<JSONObject*>(a);
        if (!p) throw std::bad_any_cast();
        return *p;
    }
    // Try const pointer
    if (a.type() == typeid(const JSONObject*)) {
        const JSONObject* p = std::any_cast<const JSONObject*>(a);
        if (!p) throw std::bad_any_cast();
        return *p;
    }
    // Try shared_ptr
    if (a.type() == typeid(std::shared_ptr<JSONObject>)) {
        auto sp = std::any_cast<std::shared_ptr<JSONObject>>(a);
        if (!sp) throw std::bad_any_cast();
        return *sp;
    }
    if (a.type() == typeid(std::shared_ptr<const JSONObject>)) {
        auto sp = std::any_cast<std::shared_ptr<const JSONObject>>(a);
        if (!sp) throw std::bad_any_cast();
        return *sp;
    }
    throw std::bad_any_cast();
}

JSONArray MiniFormat::anyToJSONArray(const std::any& a) {
    if (a.type() == typeid(JSONArray)) {
        return std::any_cast<JSONArray>(a);
    }
    if (a.type() == typeid(JSONArray*)) {
        auto p = std::any_cast<JSONArray*>(a);
        if (!p) throw std::bad_any_cast();
        return *p;
    }
    if (a.type() == typeid(const JSONArray*)) {
        auto p = std::any_cast<const JSONArray*>(a);
        if (!p) throw std::bad_any_cast();
        return *p;
    }
    if (a.type() == typeid(std::shared_ptr<JSONArray>)) {
        auto sp = std::any_cast<std::shared_ptr<JSONArray>>(a);
        if (!sp) throw std::bad_any_cast();
        return *sp;
    }
    if (a.type() == typeid(std::shared_ptr<const JSONArray>)) {
        auto sp = std::any_cast<std::shared_ptr<const JSONArray>>(a);
        if (!sp) throw std::bad_any_cast();
        return *sp;
    }
    throw std::bad_any_cast();
}

std::string MiniFormat::toBase32Upper(const std::vector<std::uint8_t>& data) {
    using boost::multiprecision::cpp_int;

    cpp_int value = 0;
    for (std::uint8_t b : data) {
        value <<= 8;
        value += b;
    }

    if (value == 0) return std::string("0");

    const unsigned base = 32;
    std::string digits;
    while (value > 0) {
        cpp_int q = value / base;
        unsigned int r = static_cast<unsigned int>((value - q * base).convert_to<unsigned long long>());
        value = q;

        char c;
        if (r < 10) c = static_cast<char>('0' + r);
        else        c = static_cast<char>('a' + (r - 10)); // 'a'..'v'
        digits.push_back(c);
    }
    std::reverse(digits.begin(), digits.end());

    // Uppercase
    std::transform(digits.begin(), digits.end(), digits.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });

    return digits;
}

#ifdef STANDALONE_TEST
// Translate Java main to C++ main
int main(int argc, char* argv[]) {
    using org::minima::utils::MiniFormat;

    std::string tester = "[{\"command\":\"balance\",\"status\":true,\"pending\":false,\"response\":[{\"token\":\"Minima\",\"tokenid\":\"0x00\",\"confirmed\":\"702.98859999729999999999999999999999608499999877\",\"unconfirmed\":\"0\",\"sendable\":\"498.98989999989999999999999999999999608499999877\",\"coins\":\"81\",\"total\":\"1000000000\"},{\"token\":{\"name\":\"neverhidden\"},\"tokenid\":\"0xF5F858AAD86199FDB6EC02F5F3EBF3E3693E26FEF075A3352F22DD8033B648A8\",\"confirmed\":\"99\",\"unconfirmed\":\"0\",\"sendable\":\"99\",\"coins\":\"1\",\"total\":\"99\"},{\"token\":{\"name\":\"herwfewr\",\"url\":\"https%3A%2F%2Fmedia.giphy.com%2Fmedia%2F18YeX8h3cTw1eKZCNZ%2Fgiphy.gif\",\"description\":\"\",\"owner\":\"\",\"webvalidate\":\"\"},\"tokenid\":\"0x03E879AC5F6063168BD8151D4502C61145ABA8CAED91F0FB9EFE943B0D5D5882\",\"confirmed\":\"123\",\"unconfirmed\":\"0\",\"sendable\":\"123\",\"coins\":\"1\",\"total\":\"123\"},{\"token\":{\"name\":\"elias\"},\"tokenid\":\"0xF5B12CBD41761C20672BCFA483F2ACC0C4A1AF4B22B1BA7A11C11D55731D1E29\",\"confirmed\":\"123\",\"unconfirmed\":\"0\",\"sendable\":\"123\",\"coins\":\"10\",\"total\":\"123\"},{\"token\":{\"name\":\"EliasToken\",\"url\":\"data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAACUAAAAyCAYAAADbTRIgAAAAAXNSR0IArs4c6QAAEjFJREFUWEdVmQmYnWV1x3%2Fvt9519iWZmYTJBiEQIBCQgEHog6VQba2aIlr1eQQUrBRbEWu1WOVBsVUqAgoCIiLVAAoVSpEdFIgECwZMDAJJJPuEyUxm7vKtb3vOd4P2Jve5c%2B%2B3nfd%2F%2Fuec%2FzmvufGG71vPL7HyuJMZGJzDrp3bufmblzP7%2B1foHRvnw5%2F8Z8YXLiRqQ2ZzyqGDvH649nbOevxGgrhJyQ%2BxSQtjcxyH4m0NruNiHIPxK1AqEWWGm80oM3PHWXHU0TiuQ63iMjnTYOPLm8i76px7zgcwX7j9XpsmCfNHF9DT1cPO7VvZvP5JSjbFuh4LV65m%2FvgiLAabg%2B%2BDYxx%2BuX4dR73yNK4ccVw8DIYM13UxBv10XIN1fYzjA5a4HTN98jvZH8VMz0zSTlKefGEDxpXj0FOv8aGz12BW3%2FJTa9Ic11o8vZmD6zj6afMcscRYdFWeQAA4noM82Xc9RcLR8z2M6%2BE4Lo7n6%2FnINZ6PLazEyPWuS25lIXIvo%2F%2BtcXCMfDW6YHPKrfdbmyTIKa6eZvUE33WxNseVG1qrhjrytxzzA12577lqUG5zPDGo8%2Fb8AOO5apQcTzF4QYAjhnkOWQbGdcixaoixcp5R463YcNIt91mbZcoHTy01uIJOYTguwg15O%2FrbQYPlAXKuICKrFncJmQQxxxHUXH27gY8r7pGHeo6iaYxDKlxwDOKMXO9r9D5q%2BFu%2B%2B1NrslwhdY3Fx8ERtAoWKVPkAl8IKy52XQJfbmzUxQq3GOOJK%2BVv4ZIYJy4sDHN8r2C%2FV3y3Rt5iTGFIroDJ04QnBrPyxnusuEJcFMqFeY4jUaTHC9cJOp483Gbq1tD3cIU3HQTFCHGdIKDGCUc6rnM8caWDVaM8jO%2BR6ZIFHgkKoYR8t2qo8MscfcNPrBgg%2FFcam5we1%2BBnCUnukss%2FsVlclWWEga9GCcH9DtyePlR%2BE44JtwSYguyOGOJ6yi%2Fk03P1XDFSjehEZhFBRYCZ4268x2ZZrqEtRjnW0nzmQXasf5xMrM%2FE4yhK5A7JVIPkQFN%2FU5dYpaZGqSJuJDKLKC3%2BNpRHB5Q7Qn7P%2BJCmtKam%2BbPrbqFv8aEaDBqNwit5r%2FjO3TZLO0TvkDpZ%2Fxjdqzz8oTrWFuzycgdvapa9P3iIbZumsbg4NiPPM3K3Ss1tUwpaHDk8wi%2B27C%2BM6rzmnHUS1aMWYDKHWlAmnI3Z%2BPDTHHn%2BJWqULEsN6wSNOer6n9g8y3DyDE84gkO8%2FjGGV1dw%2BysFfhaStmHNl2%2BjO56gMuAQllMgxdm8mB3zj2PeyF34R4R4jVUc%2BJ9HNFL1JZnd83h%2BxcncO3ICNs0IY8PLDzzDMR%2F7NP2HHqZIK0KOwfN9zJHfvssqmfNMXREYMeoJ%2Bo516Zo%2FiBG4LWRZk%2FbWKX7ztfs0VRSvnKu%2BfiVX%2FuvV7NmzW8nabSeYMcP%2FDym%2Fp8bIB89SA%2Bu1OjZJ%2Bd1%2FPcHy8z5Dnxhliiqh0SwcXHrtWltwSQ5YvNwSPfsoY6u7CYe6yXODk0tiyMkzh9bEJG889xrnv1blmcv%2BHu%2FRO3ny8ScKd%2BKSZalWAiG7rgZYeM4ZyhehiUSNiQ2%2Fe%2BRpVpz3GfqXHKpGaTb3vCJfHfWtu2ySpZokERcC7XWPMH5qF%2BFAL2QGK2EiuAhbER7Jbz42r2IzDzexZDHgBUTxDFmrRVgV0seQ5ZgsIMttEcU2J48StjzyLCvP%2F0d6Fy16k0tqmHBr%2BbV3WGvFg5Y8Swgcl%2BbTDzF6Qo3e8T5M5mLzIsFJ6pWSkuUeaewTRz4ji4%2BgZ96Ypo56OeD1537NxK7XydM2pBF%2BKdMItlZqHkgQJ1HO1kfWsfKjl9C7eAlWuCQ5S%2Bqt8OuI6%2B6wB8NeKn2WxjSfepCx43spD3VTrwqkAe0oojHbwlDGDUrUyn2878wPctdHXuKN%2F54gocGyjy5g4IuL2XDLT2g609i8Rasxi%2BsaKpWQLEk15aQJbH50Hcefdwnd4wtw%2FaKQ40hSdjGHX7u2MEqLrsHkGTNPPMjS04awtQqkFsexGhW5JEhbwWYBi4YOId24hBcufYHP3bQcm2d876vP0%2F2%2BeSy7dDkP3HYHJpoFEqIoIYliTbqZjQncEq89%2BUuO%2FvAn6VmwWCuDJFdjPM3%2B5tCrf6iZSAxzhTrWcuDnP%2BOwkwapjfTrDYUHQckhzSvkeUCeBfzte8%2Fn%2Fps286t%2F%2BAURe%2FjQxSey9urnaXKAC5r%2FxP3X3kzmJ5C3ybOijGVpSpLkSoc9v3qRoz94IT3jC4t6p5InKPi1%2BKrbrNQ1IbEaZ3OaTz3MEafOpTa3j6gdMdsoMn6tVqappO3ms%2B%2F7CGtvfpGnL7qXmCkOXz5Gewdsnfwtn5j9d%2B656SYgwtUIkIiUhRuiJMMkOVMv%2Foal77%2BQ8ug8zWOOlB9cLeRm0ddv09iSzFxCtE9G9PTDLDtlDpU5PVJfmJltk6Y1LC1KQYWW28WS7kNwDgzy1pmUtZdv4rXdzxGVWqy582OMnbiMO%2B64nbw1g8lTPIngPFPDRADNEJJseoml7%2F4QpbljKm80%2BHEIwrBwnyQzQUguEOk1%2B%2BQDHH7aPMpDAxrKUriSOCaJKURfrQsv6Odv3n4mD9%2B2nh2PbSZ2IpaedRTHrDmen937OFNT%2ByA%2BgM0STQuasjR6JRODu2MHI2eejT88UrhMlYbIHhez5Krb7UHZ61rRNBmtpx5i8SmjhMN9kCX4oqxMTt6yzDZzvCCkMjBIT%2B8iKn1zGRudS%2Bil7JmOmJ3Yw8uvvgqTO7BxU8uKGGMyqwla1pRnOf7evQyevgbTP4zvHxSIPo7vYhZ97fsCkipPgVhcGa17iEWrR%2Bga6i8cai2xzSglDu22S5RDORjCdPXilQZwBvrIuwO8yWkaM29gJydI5bPdkhZI0fEyS%2BA4TE%2FPEhEwZGBg1dvJu%2FrxvCIVaFoQo8a%2FeqvkLs3mWV5I4vYzD7HslLlU5vRpAGj%2FoOLHkrUFsYBy0E%2BTkKBvgLb1ifw21apHPrufbOYAzM7gxIleL2kmS3KiRkacGlIvZ2Gtm65jV9MOqgSBr0SXMuNLQT7kyu9qQZZ%2F0qtJHTxoVHm4R28qBmmFkVfswH5ppqrYwMcEZZwuVxOjKUPeaoAgFMU4aUYaiyE57YYQypAal5KfMm9olGDxCtKwqslTFavX0eijV9yk0VdU%2FsKw9lMPcuRpI5SHxKhCx%2BUS0lKKmg5uaogagbZcsahOP6Y93aBnTi9W0nUaYRJDuxERRami42EplQIas7lqv%2FljYwSLlpP6JdXwqun9Qt%2Bbsa8USAlCcZoQuC7RUw8x8%2Bt1nHLFRzC5q1alqSzUqGpMpyFol7SDmfVaxGlM4Flq5RJpmtGcbpJkDnmaF21W3qZerWNzQdwysXUnw%2F1z6H3bGeR%2BqVCc0gX5XqGnhi%2F%2FjgCgels45Yume%2FYx2hueU0hV4WoLZLWxlB9C3y9acnJESkspkAfKylxTdMZCWs91qJQCFYrtKNFWStySZZa5q%2F%2BE8qrTyL3gzbonRhlx49AXr7eBCPlOOyWNw6H7t3BiKaOrt0dbLGkWJBh27dnLYF8ftWql02IV3Yis6qBylG46yXI2b93OnP4eRod6iZKU13dPcsjcAUq%2BS7PZ5AXqbKzPI3eFT6F2OtJMSJdkBv7lW9JD4xtHe7rUZiyZ%2BB2ryjldPd0azoEv3aBh1549DIhRlbJyUBDWNl%2B6FMFDWyaI44grr%2F0%2B7zj9ZE5auZx9b0zx6LoNvOdPV1EuBZp%2Bwu5u3N5eEuNx92TALirYTo9our5wnS250mW4ZMZyVN3jrZOb2D0xRalaUwR0ViC%2BUd1TTFPEgDhO8FyjIb17Yi%2FdXSJ1ylodZmeb1GsVSqG0%2BBAnCT31qp6fZynL5w9z%2BPiwJHcenAr49kQZI9Esta%2FvsmutzAYkkhZUXK5ZXiXwHNY%2B%2FAv%2B7YYfYRsznTZKhF7hZAlJ7aG1veocVubl2kyKmrPGau4Jy2X6ursZHuilr7eb3t4eeuol3vW2Y1i94jCN7l9Ou3zzjSoHCEB41fP5q22lVOG4Ho8vH1kjdAybtu3k%2Fqd%2BxbYduzSaJGKkBqZpospTvif6e65IijTRmiYpo6NiJQCEDqLL1a0CNBCGAfOH%2B7n4nD%2Fn2GXjpGmuAXTFzoAnZ0tanM3H73zANk3A%2B4darOxWHIjznEazxWyz8Qc0BBFFyKgmuv72e3jLETkTrRGmGyVe3LSJJMtIkoxKpaSBcfThC5X0cSJOshy7ZB6LR4XsAYfMGVBjiq7acM3ukPtmQoykhE89uN6OpdO8Z7DFUD38oyZXIqoYfr0ZYQdbb9GTCdg4VUUZRzH7Ww2mvFnaSZOZbJaF6RgLDh8vpIFMXXRCUww0ZEalLbzkB4EwCJjesZ87nMPIFamfPmlX2L0cZ%2FZRCqVDES9YatWi31PNLIMxGVpI%2Fy8tktzLHBx8FV2OijRRJXlWjIvCEMe4eF6gGt31ffwgxISh%2BFC5mDSa2jTIbIHZBs%2Bu36YBZy740X3284szap4hLIU0GhGNZpMRKcaaCDsTFJUXnmZ0beqytBhadLgkBFXNJCioVunME%2BS7ljCZFXQGHUKDJKExuV%2BzvqiT6uAA1%2Byo63nm4tt%2FbC9b6uJ6jo4LBQYhaJRaPnn5N7BRRJzD2NioIjjS30ur1eSV7bt0%2FFMKPAYH%2Btmzdx9ZmnPqiSt4fP3z2kvJ5KWrVmN6dlYX8%2F417%2BI%2F7r6PY5Yv5aPnvlcXJfPWidd3MnfhOFe9FpJ4Ieblp%2B%2B2%2FaHBU%2FgLov%2Fs8fVc8a1beedpJ3HPE88WUaVR4nHWqSfzwsbNXHDOX%2FGl676rI6I4ihgd7eelTb%2BnWipx9hnH8Z9PbCDPU6qVKp%2B68AI%2B%2FaWvaEHu6%2BnhhBXLufrLFxWoymRXEXZo7drNDY35mN3P3GXFbeJ3CWffcdm1b5oLP%2F91kriN44dMvDGpKeAdp53Ei5teZXB4kE2vbCOLE3bt20dfV52Pf%2BAveODnzzI5dUC19pq%2FPIubblvLsUccRq0UsGXnbk5fdaz2f0034NILzy6iRdxeqyuSja3budk%2FHLNn3Y%2BtrKherythwyBgwyvbeGnrdqJmwmGHLuDlLdu4%2FsYf6pyg1WzwztNOZmKmweJFC6h297Nzy1YGuusM9VV4ddsOgpIETFGe9s%2B0eXbjZubNGWB8ziC%2BF7Jq9fGccerKgo%2BCVLmstGlNTvO91nwx6k4rbpG3DBeCUKq6xfGNcrpcreqFm7Zs1wiWeUFQ6qJaq%2BMnOb%2Fd%2BAovvLBBkZyZ2k%2BaJ8rpdqtNmqTs2rWTyy67qBjMyugwzxno76GvrxuSFEoh7QMHNF%2BFvT3cPDGM%2BcE3PmfLlZq6z%2FMlAyd4jke1UiaKIqrlkGoppFatMrpsGWGtVIxoBPpGQ0sOMqqqlIoxrzxIBvJxXERdKcRGxd%2FSNYk0Fu2kBUpG33G7iGrRVGHAnukc84m%2FO89WApdKuaRR19tT1wLbnm1oLhEXpHGMcTy8QKIl02QatdsqyMQFMjWWEBmYO0Stu97JWTrr1eDJskxH3olAr%2BMBGZpknXRQpASJ%2FHKpRH1sIYb6kDVxpFNbUej9g4PqvgNT%2B0nSXA2MWi1NnOJGrW2Cjszeg6BASuS05zA8Po%2BBkTkq7nTlukkj7ZnsPHhKISkpSZLoLkaeZKoixNh6dzfd3T2c%2Fp53Y%2BgZs0zt1da6uLvI32JK92YC1FmrqE7dKiiGYVoixCBJjgX8%2BruOvXXfpDM7%2F0MSfXPrQ4YWMuPyXB0H2DBUfTU8OsLF3%2FimYD5uacdFJERNJbIuQ0gggwnpcA9mZx3Pya5RAFFU5JmD21ZyvZ7XMUjDXdxb7O%2BIskQaUykxckwWEYo%2BL4Ao9fdy5Mkn8tcXX4Jh6QnFoF9kSSwnC1lj3QozmQy8ilmohqLst%2BgotzP0FNep6uzsGGhX20FKZqjiQt9X0afDVtk8ytJiK0TuJdFoHMJqhbBSZuSQ%2BZx76WcwZ15zqxU5GwQeTm4p%2BT4H3tjH5o0b2fP6dtqN2WKEEwmxJckWuwVJEmvxlOZCB%2F%2FGaEDIw2UiKJtGByVymqZUymVi4aGO3Dua3nXxPZ9yuUS5XqNUqXDuRRdhPvv8q1b35pToIn3lphkNqVdZWnQr4nfZ31GEZFukcJOIN0FBrpMBRdGqSX7LdP9G0ZFND1UYbhGlco%2B8GNAJ9TxhlaoQCAOPrv%2FLi%2F8L95RBKGaZbG8AAAAASUVORK5CYII%3D\",\"description\":\"\",\"ticker\":\"ELS\",\"webvalidate\":\"\"},\"tokenid\":\"0xEF536EC66C51EFDFA207065C3A1F67AFBB7FDFEE0DEDEFB237A7FB7FD96EB69A\",\"confirmed\":\"1000\",\"unconfirmed\":\"0\",\"sendable\":\"1000\",\"coins\":\"1\",\"total\":\"1000\"},{\"token\":{\"name\":\"{\\\"name\\\":\\\"EliasToken\\\",\\\"url\\\":\\\"data:image\\/png\"},\"tokenid\":\"0xB3FEF50070166CEAF8F9CDBC45F8142E7E1D93F35967F5415E388DFE0A0BCCCA\",\"confirmed\":\"1000\",\"unconfirmed\":\"0\",\"sendable\":\"1000\",\"coins\":\"1\",\"total\":\"1000\"},{\"token\":{\"name\":\"gg\"},\"tokenid\":\"0x6C1E89B281FD5BDFC581D47E37751E3E81DD60F8E25D89670BBE4C483CDA3F02\",\"confirmed\":\"123\",\"unconfirmed\":\"0\",\"sendable\":\"123\",\"coins\":\"1\",\"total\":\"123\"},{\"token\":{\"name\":\"testing\"},\"tokenid\":\"0x17F5C34C755C8F59E31DE73957B01F588AAB18AF26826E4828844C19401BF61B\",\"confirmed\":\"123\",\"unconfirmed\":\"0\",\"sendable\":\"123\",\"coins\":\"1\",\"total\":\"123\"},{\"token\":{\"name\":\"TEST\"},\"tokenid\":\"0x4C4BFFDE7C1A46FD36767E56E9260487A27BF2717990E74DAC1BFE73C87C5FE6\",\"confirmed\":\"5\",\"unconfirmed\":\"0\",\"sendable\":\"5\",\"coins\":\"1\",\"total\":\"5\"},{\"token\":{\"name\":\"jimmy2\"},\"tokenid\":\"0x93F2AF1236F5E076B38B4716DB9B6B2E81C1D5DE41EBCEFDA28329AE2D772058\",\"confirmed\":\"323\",\"unconfirmed\":\"0\",\"sendable\":\"323\",\"coins\":\"1\",\"total\":\"323\"},{\"token\":{\"name\":\"jimmy\"},\"tokenid\":\"0x206AE09BE76F9E1D2B8D92AEDDC12BA85CD152F3D037349ADDF853F2C53EDF98\",\"confirmed\":\"300\",\"unconfirmed\":\"0\",\"sendable\":\"300\",\"coins\":\"1\",\"total\":\"300\"},{\"token\":{\"name\":\"jimyyboy\"},\"tokenid\":\"0xAE0D72FE6055E6A06D38252D4255D58BF82A5BCAC1E457CB0F42275BBBAF7B0D\",\"confirmed\":\"12\",\"unconfirmed\":\"0\",\"sendable\":\"12\",\"coins\":\"1\",\"total\":\"12\"},{\"token\":{\"name\":\"elias\"},\"tokenid\":\"0x849A613949DE85BBB841F312E1B19A3000D08F0388EC79FAA99CD0918F620573\",\"confirmed\":\"777\",\"unconfirmed\":\"0\",\"sendable\":\"777\",\"coins\":\"2\",\"total\":\"777\"},{\"token\":{\"name\":\"TokenB\",\"url\":\"\",\"description\":\"\",\"ticker\":\"\",\"webvalidate\":\"\"},\"tokenid\":\"0xD96D0CF90A1A828D1B1C61D3B66D12D51AC982633A2D13A7C87FA502135E61E1\",\"confirmed\":\"10\",\"unconfirmed\":\"0\",\"sendable\":\"10\",\"coins\":\"2\",\"total\":\"10\"},{\"token\":{\"name\":\"TokenA\",\"url\":\"\",\"description\":\"\",\"ticker\":\"\",\"webvalidate\":\"\"},\"tokenid\":\"0x1A3990B91A2E94C84DCCBC77808EF02CEE0B66C434B67AA765BB9C26D49D23F1\",\"confirmed\":\"10\",\"unconfirmed\":\"0\",\"sendable\":\"9\",\"coins\":\"11\",\"total\":\"10\"}]}]\n";

    try {
        JSONArray obj = MiniFormat::convertToJSON(tester);
        std::string ver = MiniFormat::JSONPretty(obj, true);
        std::cout << ver << std::endl;
    } catch (const ParseException& pe) {
        std::cerr << "ParseException: " << pe.getMessage() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    }
    return 0;
}
#endif // STANDALONE_TEST

} // namespace utils
} // namespace minima
} // namespace org