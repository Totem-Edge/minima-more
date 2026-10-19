#include "org/minima/system/commands/mcp/mcp_server.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_value.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace mcp {

using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONValue;

// ---- SSE sink (server -> client stream) ----
struct SseSink {
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::string> q;
    bool closed = false;
};

namespace {
std::mutex g_sseMutex;
std::vector<std::weak_ptr<SseSink>> g_sseSinks;
} // anonymous namespace

std::shared_ptr<SseSink> McpServer::openSseSink() {
    auto sink = std::make_shared<SseSink>();
    std::lock_guard<std::mutex> lk(g_sseMutex);
    g_sseSinks.erase(std::remove_if(g_sseSinks.begin(), g_sseSinks.end(),
                                    [](const std::weak_ptr<SseSink>& w) { return w.expired(); }),
                     g_sseSinks.end());
    g_sseSinks.push_back(sink);
    return sink;
}

void McpServer::closeSseSink(const std::shared_ptr<SseSink>& sink) {
    if (!sink) return;
    {
        std::lock_guard<std::mutex> lk(sink->m);
        sink->closed = true;
    }
    sink->cv.notify_all();
    std::lock_guard<std::mutex> lk(g_sseMutex);
    g_sseSinks.erase(std::remove_if(g_sseSinks.begin(), g_sseSinks.end(),
                                    [&](const std::weak_ptr<SseSink>& w) {
                                        auto s = w.lock();
                                        return !s || s == sink;
                                    }),
                     g_sseSinks.end());
}

void McpServer::broadcast(const std::string& jsonrpcNotification) {
    std::vector<std::shared_ptr<SseSink>> sinks;
    {
        std::lock_guard<std::mutex> lk(g_sseMutex);
        for (auto& w : g_sseSinks) if (auto s = w.lock()) sinks.push_back(s);
    }
    for (auto& s : sinks) {
        {
            std::lock_guard<std::mutex> lk(s->m);
            if (s->closed) continue;
            s->q.push_back(jsonrpcNotification);
        }
        s->cv.notify_one();
    }
}

bool McpServer::nextSseMessage(const std::shared_ptr<SseSink>& sink,
                               std::string& out, int timeoutMs) {
    if (!sink) return false;
    std::unique_lock<std::mutex> lk(sink->m);
    sink->cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                      [&] { return sink->closed || !sink->q.empty(); });
    if (!sink->q.empty()) {
        out = std::move(sink->q.front());
        sink->q.pop_front();
        return true;
    }
    return false;
}

std::size_t McpServer::sseSinkCount() {
    std::lock_guard<std::mutex> lk(g_sseMutex);
    std::size_t n = 0;
    for (auto& w : g_sseSinks) if (!w.expired()) ++n;
    return n;
}

namespace {

constexpr const char* kProtocolVersion = "2024-11-05";

// ---------------------------------------------------------------------------
// MiniJson: a small, self-contained JSON parser. The node's generated lexer
// (JSONParser/Yylex) returns EOF for every input, so MCP requests cannot use it.
// This handles the standard JSON subset (objects, arrays, strings with escapes
// and \uXXXX, numbers, booleans, null) which is all JSON-RPC needs.
// ---------------------------------------------------------------------------
struct MJValue {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    std::string s;                 // Str: decoded bytes; Num: raw text
    std::vector<MJValue> arr;
    std::vector<std::pair<std::string, MJValue>> obj;

    const MJValue* get(const std::string& k) const {
        for (const auto& kv : obj) if (kv.first == k) return &kv.second;
        return nullptr;
    }
};

void appendUtf8(std::string& out, unsigned cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class MiniJson {
public:
    explicit MiniJson(const std::string& text) : t(text) {}

    bool parse(MJValue& out) {
        i = 0;
        ws();
        if (!value(out)) return false;
        ws();
        return i == t.size();
    }

private:
    const std::string& t;
    std::size_t i = 0;

    void ws() {
        while (i < t.size()) {
            char c = t[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else break;
        }
    }
    bool isDigit(char c) const { return c >= '0' && c <= '9'; }

    bool value(MJValue& v) {
        ws();
        if (i >= t.size()) return false;
        char c = t[i];
        if (c == '{') return object(v);
        if (c == '[') return array(v);
        if (c == '"') { v.type = MJValue::Str; return string(v.s); }
        if (c == 't') { if (t.compare(i, 4, "true") != 0) return false; i += 4; v.type = MJValue::Bool; v.b = true; return true; }
        if (c == 'f') { if (t.compare(i, 5, "false") != 0) return false; i += 5; v.type = MJValue::Bool; v.b = false; return true; }
        if (c == 'n') { if (t.compare(i, 4, "null") != 0) return false; i += 4; v.type = MJValue::Null; return true; }
        return number(v);
    }

    bool number(MJValue& v) {
        std::size_t st = i;
        if (i < t.size() && t[i] == '-') ++i;
        if (i >= t.size() || !isDigit(t[i])) return false;
        if (t[i] == '0') ++i;
        else while (i < t.size() && isDigit(t[i])) ++i;
        if (i < t.size() && t[i] == '.') {
            ++i;
            if (i >= t.size() || !isDigit(t[i])) return false;
            while (i < t.size() && isDigit(t[i])) ++i;
        }
        if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
            ++i;
            if (i < t.size() && (t[i] == '+' || t[i] == '-')) ++i;
            if (i >= t.size() || !isDigit(t[i])) return false;
            while (i < t.size() && isDigit(t[i])) ++i;
        }
        v.type = MJValue::Num;
        v.s = t.substr(st, i - st);
        return true;
    }

    bool string(std::string& out) {
        if (i >= t.size() || t[i] != '"') return false;
        ++i;
        out.clear();
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c != '\\') { out.push_back(c); continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = 0;
                    if (i + 4 > t.size()) return false;
                    for (int k = 0; k < 4; ++k) {
                        char h = t[i++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp += static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp += static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp += static_cast<unsigned>(h - 'A' + 10);
                        else return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= t.size() &&
                        t[i] == '\\' && t[i + 1] == 'u') {
                        i += 2;
                        unsigned lo = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = t[i++];
                            lo <<= 4;
                            if (h >= '0' && h <= '9') lo += static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') lo += static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') lo += static_cast<unsigned>(h - 'A' + 10);
                            else return false;
                        }
                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool array(MJValue& v) {
        ++i;  // [
        v.type = MJValue::Arr;
        ws();
        if (i < t.size() && t[i] == ']') { ++i; return true; }
        while (true) {
            MJValue e;
            if (!value(e)) return false;
            v.arr.push_back(std::move(e));
            ws();
            if (i < t.size() && t[i] == ',') { ++i; continue; }
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            return false;
        }
    }

    bool object(MJValue& v) {
        ++i;  // {
        v.type = MJValue::Obj;
        ws();
        if (i < t.size() && t[i] == '}') { ++i; return true; }
        while (true) {
            ws();
            std::string key;
            if (!string(key)) return false;
            ws();
            if (i >= t.size() || t[i] != ':') return false;
            ++i;
            MJValue e;
            if (!value(e)) return false;
            v.obj.emplace_back(std::move(key), std::move(e));
            ws();
            if (i < t.size() && t[i] == ',') { ++i; continue; }
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            return false;
        }
    }
};

std::string mjQuote(const std::string& s) {
    return std::string("\"") + JSONValue::escape(s) + "\"";
}

std::string mjSerialize(const MJValue& v) {
    switch (v.type) {
        case MJValue::Null: return "null";
        case MJValue::Bool: return v.b ? "true" : "false";
        case MJValue::Num:  return v.s;
        case MJValue::Str:  return mjQuote(v.s);
        case MJValue::Arr: {
            std::string o = "[";
            for (std::size_t k = 0; k < v.arr.size(); ++k) {
                if (k) o += ",";
                o += mjSerialize(v.arr[k]);
            }
            return o + "]";
        }
        case MJValue::Obj: {
            std::string o = "{";
            for (std::size_t k = 0; k < v.obj.size(); ++k) {
                if (k) o += ",";
                o += mjQuote(v.obj[k].first) + ":" + mjSerialize(v.obj[k].second);
            }
            return o + "}";
        }
    }
    return "null";
}

std::string mjId(const MJValue& v) {
    if (v.type == MJValue::Str) return mjQuote(v.s);
    if (v.type == MJValue::Num) return v.s;
    return "null";
}

std::string mjArgText(const MJValue& v) {
    if (v.type == MJValue::Str) return v.s;
    if (v.type == MJValue::Num) return v.s;
    if (v.type == MJValue::Bool) return v.b ? "true" : "false";
    return mjSerialize(v);
}

// ---- manifest/registry helpers -------------------------------------------

// Unwrap a JSON value that may be stored by value or shared_ptr.
bool asObjectAny(const std::any& v, JSONObject& out) {
    if (const auto* o = std::any_cast<JSONObject>(&v)) { out = *o; return true; }
    if (const auto* sp = std::any_cast<std::shared_ptr<JSONObject>>(&v)) {
        if (*sp) { out = **sp; return true; }
    }
    return false;
}

std::string jsonTextOfAny(const std::any& v) {
    if (const auto* o = std::any_cast<JSONObject>(&v)) return o->toString();
    if (const auto* sp = std::any_cast<std::shared_ptr<JSONObject>>(&v)) { if (*sp) return (*sp)->toString(); }
    if (const auto* a = std::any_cast<JSONArray>(&v)) return a->toString();
    if (const auto* spa = std::any_cast<std::shared_ptr<JSONArray>>(&v)) { if (*spa) return (*spa)->toString(); }
    return JSONValue::toJSONString(v);
}

std::string jsonTextOfArray(const JSONArray& arr) {
    std::string s = "[";
    bool first = true;
    for (const auto& e : arr.elements()) {
        if (!first) s += ",";
        first = false;
        s += jsonTextOfAny(e);
    }
    s += "]";
    return s;
}

std::string schemaType(const std::string& t) {
    if (t == "number" || t == "int" || t == "integer") return "integer";
    if (t == "bool" || t == "boolean") return "boolean";
    return "string";
}

JSONObject toolFromCommand(const JSONObject& c) {
    JSONObject tool;
    tool.put("name", c.getString("name"));
    tool.put("description", c.getString("summary"));

    JSONObject schema;
    schema.put("type", std::string("object"));
    JSONObject props;
    JSONArray required;

    if (c.containsKey("params")) {
        const std::any& anyParams = c.get("params");
        if (const auto* arr = std::any_cast<JSONArray>(&anyParams)) {
            for (const auto& pe : arr->elements()) {
                JSONObject po;
                if (!asObjectAny(pe, po)) continue;
                const std::string pname = po.getString("name");
                if (pname.empty()) continue;

                JSONObject prop;
                prop.put("type", schemaType(po.containsKey("type") ? po.getString("type") : "string"));
                if (po.containsKey("description")) prop.put("description", po.getString("description"));
                if (po.containsKey("enum")) prop.put("enum", po.get("enum"));
                props.put(pname, prop);

                if (po.containsKey("required") && po.getBoolean("required")) required.add(pname);
            }
        }
    }
    schema.put("properties", props);
    if (required.size() > 0) schema.put("required", required);
    tool.put("inputSchema", schema);

    if (c.containsKey("annotations")) {
        const std::any& ann = c.get("annotations");
        JSONObject ao;
        if (asObjectAny(ann, ao)) tool.put("annotations", ao);
    }
    return tool;
}

JSONObject toolsList() {
    JSONObject result;
    JSONArray tools;

    std::unique_ptr<JSONObject> manifest = CommandRunner::capabilityManifest();
    if (manifest && manifest->containsKey("commands")) {
        const std::any& anyCmds = manifest->get("commands");
        if (const auto* arr = std::any_cast<JSONArray>(&anyCmds)) {
            for (const auto& ce : arr->elements()) {
                JSONObject co;
                if (!asObjectAny(ce, co)) continue;
                tools.add(toolFromCommand(co));
            }
        }
    }
    result.put("tools", tools);
    return result;
}

JSONObject toolsCall(const MJValue& params) {
    JSONObject result;
    JSONArray content;
    bool isError = false;

    const MJValue* namev = params.get("name");
    const std::string name = (namev && namev->type == MJValue::Str) ? namev->s : "";
    if (name.empty()) {
        isError = true;
        JSONObject c; c.put("type", std::string("text"));
        c.put("text", std::string("tools/call: missing tool name"));
        content.add(c);
        result.put("content", content);
        result.put("isError", isError);
        return result;
    }

    // Compose the command string: "<name> key:value key:value".
    std::string cmd = name;
    if (const MJValue* args = params.get("arguments")) {
        if (args->type == MJValue::Obj) {
            for (const auto& kv : args->obj) {
                cmd += " " + kv.first + ":" + mjArgText(kv.second);
            }
        }
    }

    std::string text = "{}";
    try {
        auto runner = CommandRunner::getRunner();
        if (runner) {
            std::unique_ptr<JSONArray> res = runner->runMultiCommand(cmd);
            if (res) {
                text = jsonTextOfArray(*res);
                const auto& elems = res->elements();
                if (elems.size() == 1) {
                    JSONObject o;
                    if (asObjectAny(elems[0], o) && o.containsKey("status")) {
                        isError = !o.getBoolean("status");
                    }
                }
            } else {
                isError = true;
                text = "{\"error\":\"command not found\"}";
            }
        } else {
            isError = true;
            text = "{\"error\":\"command runner unavailable\"}";
        }
    } catch (const std::exception& e) {
        isError = true;
        text = std::string("{\"error\":\"") + JSONValue::escape(e.what()) + "\"}";
    }

    JSONObject c;
    c.put("type", std::string("text"));
    c.put("text", text);
    content.add(c);
    result.put("content", content);
    result.put("isError", isError);
    return result;
}

std::string rpcResult(const std::string& idJson, const JSONObject& result) {
    return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + idJson + ",\"result\":" + result.toString() + "}";
}

std::string rpcError(const std::string& idJson, int code, const std::string& message) {
    return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + idJson + ",\"error\":{\"code\":" +
           std::to_string(code) + ",\"message\":\"" + JSONValue::escape(message) + "\"}}";
}

} // anonymous namespace

std::string McpServer::handleLine(const std::string& line) {
    if (line.find_first_not_of(" \t\r\n") == std::string::npos) return "";

    MJValue req;
    {
        MiniJson parser(line);
        if (!parser.parse(req) || req.type != MJValue::Obj) return rpcError("null", -32700, "parse error");
    }

    // Notifications (no id) must not be answered.
    const MJValue* idv = req.get("id");
    const bool isNotification = (idv == nullptr);
    const std::string idJson = isNotification ? "null" : mjId(*idv);
    const MJValue* mv = req.get("method");
    const std::string method = (mv && mv->type == MJValue::Str) ? mv->s : "";

    if (method == "initialize") {
        JSONObject result;
        result.put("protocolVersion", std::string(kProtocolVersion));
        JSONObject caps;
        caps.put("tools", JSONObject());
        result.put("capabilities", caps);
        JSONObject serverInfo;
        serverInfo.put("name", std::string("minima-more"));
        serverInfo.put("version", org::minima::system::params::GlobalParams::getFullMicroVersion());
        result.put("serverInfo", serverInfo);
        return rpcResult(idJson, result);
    }
    if (method == "notifications/initialized" || method == "initialized") return "";
    if (method == "ping") return rpcResult(idJson, JSONObject());
    if (method == "tools/list") return rpcResult(idJson, toolsList());

    if (method == "tools/call") {
        const MJValue* pv = req.get("params");
        MJValue empty;
        const MJValue& params = (pv && pv->type == MJValue::Obj) ? *pv : empty;
        return rpcResult(idJson, toolsCall(params));
    }

    if (isNotification) return "";
    return rpcError(idJson, -32601, std::string("method not found: ") + method);
}

void McpServer::runStdio() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string resp = handleLine(line);
        if (!resp.empty()) {
            std::cout << resp << std::endl;
            std::cout.flush();
        }
    }
}

} // namespace mcp
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
