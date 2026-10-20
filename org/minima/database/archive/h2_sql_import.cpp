#include "org/minima/database/archive/h2_sql_import.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace database {
namespace archive {

namespace {

std::string upperCopy(const std::string& s) {
    std::string o = s;
    for (char& c : o) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
}

struct Value {
    enum Type { Null, Blob, Str, Num } type = Null;
    std::vector<std::uint8_t> blob;
    std::string str;
    long long num = 0;
};

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool decodeHex(const std::string& in, std::vector<std::uint8_t>& out) {
    if (in.size() % 2 != 0) return false;
    out.clear();
    out.reserve(in.size() / 2);
    for (std::size_t i = 0; i + 1 < in.size(); i += 2) {
        int hi = hexVal(in[i]), lo = hexVal(in[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

class TupleParser {
public:
    explicit TupleParser(const std::string& s) : mS(s) {}

    bool readTuples(std::size_t pos, std::vector<std::vector<Value>>& out) {
        mI = pos;
        while (mI < mS.size()) {
            ws();
            if (mI >= mS.size() || mS[mI] != '(') break;
            ++mI;
            std::vector<Value> row;
            while (true) {
                ws();
                if (mI >= mS.size()) return false;
                if (mS[mI] == ')') { ++mI; break; }
                Value v;
                if (!readValue(v)) return false;
                row.push_back(std::move(v));
                ws();
                if (mI >= mS.size()) return false;
                if (mS[mI] == ',') { ++mI; continue; }
                if (mS[mI] == ')') { ++mI; break; }
                return false;
            }
            out.push_back(std::move(row));
            ws();
            if (mI >= mS.size()) break;
            if (mS[mI] == ',') { ++mI; continue; }
            break;
        }
        return true;
    }

private:
    const std::string& mS;
    std::size_t mI = 0;

    void ws() {
        while (mI < mS.size() && std::isspace(static_cast<unsigned char>(mS[mI]))) ++mI;
    }

    bool parseHexLiteral(Value& v) {
        ++mI;  // X
        if (mI >= mS.size() || mS[mI] != '\'') return false;
        ++mI;
        std::string hex;
        while (mI < mS.size() && mS[mI] != '\'') hex.push_back(mS[mI++]);
        if (mI >= mS.size()) return false;
        ++mI;
        v.type = Value::Blob;
        return decodeHex(hex, v.blob);
    }

    bool readValue(Value& v) {
        ws();
        if (mI >= mS.size()) return false;
        if ((mS[mI] == 'X' || mS[mI] == 'x') && mI + 1 < mS.size() && mS[mI + 1] == '\'') {
            return parseHexLiteral(v);
        }
        if (mS[mI] == '\'') {
            ++mI;
            std::string str;
            while (mI < mS.size()) {
                char c = mS[mI++];
                if (c == '\'') {
                    if (mI < mS.size() && mS[mI] == '\'') { str.push_back('\''); ++mI; continue; }
                    break;
                }
                str.push_back(c);
            }
            v.type = Value::Str;
            v.str = std::move(str);
            return true;
        }
        if (mI + 4 <= mS.size() && upperCopy(mS.substr(mI, 4)) == "CAST") {
            std::size_t open = mS.find('(', mI);
            if (open == std::string::npos) return false;
            std::size_t x = mS.find_first_of("Xx", open);
            if (x == std::string::npos) return false;
            mI = x;
            if (mI + 1 >= mS.size() || mS[mI + 1] != '\'') return false;
            return parseHexLiteral(v);
        }
        if (mI + 4 <= mS.size() && upperCopy(mS.substr(mI, 4)) == "NULL") {
            mI += 4;
            v.type = Value::Null;
            return true;
        }
        std::size_t start = mI;
        if (mS[mI] == '-' || mS[mI] == '+') ++mI;
        bool digits = false;
        while (mI < mS.size() && std::isdigit(static_cast<unsigned char>(mS[mI]))) { ++mI; digits = true; }
        if (digits) {
            v.type = Value::Num;
            v.num = std::stoll(mS.substr(start, mI - start));
            return true;
        }
        start = mI;
        while (mI < mS.size() && mS[mI] != ',' && mS[mI] != ')') ++mI;
        v.type = Value::Str;
        v.str = mS.substr(start, mI - start);
        return true;
    }
};

struct Columns {
    std::vector<std::string> names;
    int indexOf(const std::string& want) const {
        for (std::size_t i = 0; i < names.size(); ++i) if (names[i] == want) return static_cast<int>(i);
        return -1;
    }
};

bool parseCreateColumns(const std::string& stmt, const std::string& tableUpper, Columns& cols) {
    const std::string up = upperCopy(stmt);
    std::size_t t = up.find(tableUpper);
    if (t == std::string::npos) return false;
    std::size_t p = stmt.find('(', t);
    if (p == std::string::npos) return false;
    int depth = 0;
    for (std::size_t i = p; i < stmt.size(); ++i) {
        char c = stmt[i];
        if (c == '(') { ++depth; continue; }
        if (c == ')') { --depth; if (depth == 0) break; continue; }
        if (c == '"' && depth == 1) {
            std::size_t j = i + 1;
            std::string id;
            while (j < stmt.size() && stmt[j] != '"') id.push_back(stmt[j++]);
            if (!id.empty()) cols.names.push_back(upperCopy(id));
            i = j;
        }
    }
    return !cols.names.empty();
}

bool parseInsertColumns(const std::string& stmt, std::size_t afterTable, std::size_t valuesPos,
                        Columns& cols) {
    std::size_t p = stmt.find('(', afterTable);
    if (p == std::string::npos || p > valuesPos) return false;
    std::size_t close = stmt.find(')', p);
    if (close == std::string::npos || close > valuesPos) return false;
    std::string inner = stmt.substr(p + 1, close - p - 1);
    std::string up = upperCopy(inner);
    std::size_t i = 0;
    while (i < up.size()) {
        std::size_t q = up.find('"', i);
        if (q == std::string::npos) break;
        std::size_t e = up.find('"', q + 1);
        if (e == std::string::npos) break;
        cols.names.push_back(upperCopy(inner.substr(q + 1, e - q - 1)));
        i = e + 1;
    }
    return !cols.names.empty();
}

void splitStatements(std::string& buffer, std::vector<std::string>& out) {
    std::size_t start = 0;
    bool inStr = false;
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];
        if (c == '\'') {
            if (inStr && i + 1 < buffer.size() && buffer[i + 1] == '\'') { ++i; continue; }
            inStr = !inStr;
        } else if (c == ';' && !inStr) {
            out.push_back(buffer.substr(start, i - start + 1));
            start = i + 1;
        }
    }
    buffer.erase(0, start);
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

H2SqlImportResult h2SqlToRawArchive(const std::filesystem::path& zH2Gzip,
                                    const std::filesystem::path& zRawGzipOut) {
    using org::minima::objects::TxBlock;
    using org::minima::objects::base::MiniByte;
    using org::minima::objects::base::MiniNumber;

    H2SqlImportResult result;

    std::filesystem::path sql = zH2Gzip;
    sql += ".h2.sql.tmp";
    org::minima::utils::MiniFile::decompressGzipFile(zH2Gzip, sql);
    std::error_code ec;
    if (!std::filesystem::exists(sql, ec) || std::filesystem::file_size(sql, ec) == 0) {
        std::filesystem::remove(sql, ec);
        throw std::runtime_error("not a gzip-compressed H2 SQL script: " + zH2Gzip.string());
    }
    const std::string text = readFile(sql);
    std::filesystem::remove(sql, ec);

    Columns syncCols, cascadeCols;
    std::vector<std::unique_ptr<TxBlock>> blocks;
    std::unique_ptr<org::minima::database::cascade::Cascade> cascade;
    long long bestCascadeTip = -1;

    std::string buffer;
    std::vector<std::string> statements;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;

        bool inStr = false;
        for (std::size_t i = 0; i + 1 < line.size(); ++i) {
            if (line[i] == '\'') inStr = !inStr;
            if (!inStr && line[i] == '-' && line[i + 1] == '-') { line.erase(i); break; }
        }

        buffer += line;
        buffer.push_back('\n');
        splitStatements(buffer, statements);

        for (const auto& stmt : statements) {
            const std::string up = upperCopy(stmt);
            if (up.find("CREATE") != std::string::npos && up.find("SYNCBLOCK") != std::string::npos) {
                parseCreateColumns(stmt, "SYNCBLOCK", syncCols);
            } else if (up.find("CREATE") != std::string::npos &&
                       up.find("CASCADEDATA") != std::string::npos) {
                parseCreateColumns(stmt, "CASCADEDATA", cascadeCols);
            } else if (up.find("INSERT") != std::string::npos) {
                const bool isSync = up.find("SYNCBLOCK") != std::string::npos;
                const bool isCasc = up.find("CASCADEDATA") != std::string::npos;
                if (!isSync && !isCasc) continue;

                const std::string table = isSync ? "SYNCBLOCK" : "CASCADEDATA";
                std::size_t tpos = up.find(table);
                std::size_t vpos = up.find("VALUES", tpos);
                if (vpos == std::string::npos) continue;

                Columns insertCols;
                const bool explicitCols = parseInsertColumns(stmt, tpos + table.size(), vpos, insertCols);
                const Columns& cols = explicitCols ? insertCols : (isSync ? syncCols : cascadeCols);
                const std::string want = isSync ? "SYNCDATA" : "FULLDATA";
                int blobIdx = cols.indexOf(want);
                if (blobIdx < 0) blobIdx = isSync ? 4 : 2;
                const int tipIdx = isCasc ? (explicitCols ? insertCols.indexOf("CASCADETIP")
                                                          : cascadeCols.indexOf("CASCADETIP"))
                                          : -1;

                std::vector<std::vector<Value>> tuples;
                TupleParser parser(stmt);
                if (!parser.readTuples(stmt.find('(', vpos), tuples)) continue;

                for (const auto& row : tuples) {
                    if (blobIdx < 0 || static_cast<std::size_t>(blobIdx) >= row.size()) continue;
                    const Value& val = row[blobIdx];
                    if (val.type != Value::Blob || val.blob.empty()) continue;

                    std::string bytes(reinterpret_cast<const char*>(val.blob.data()), val.blob.size());
                    std::istringstream iss(bytes, std::ios::binary);
                    try {
                        if (isSync) {
                            auto blk = TxBlock::ReadFromStream(iss);
                            if (blk) blocks.push_back(std::move(blk));
                        } else {
                            auto casc = org::minima::database::cascade::Cascade::ReadFromStream(iss);
                            if (casc) {
                                long long tip = -1;
                                if (tipIdx >= 0 && static_cast<std::size_t>(tipIdx) < row.size() &&
                                    row[tipIdx].type == Value::Num) {
                                    tip = row[tipIdx].num;
                                }
                                if (!cascade || bestCascadeTip < 0 || tip < bestCascadeTip) {
                                    bestCascadeTip = tip;
                                    cascade = std::move(casc);
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        org::minima::utils::MinimaLogger::log(
                            std::string("H2 import: skipping malformed ") + (isSync ? "block" : "cascade") +
                            " row: " + e.what());
                    }
                }
            }
        }
        statements.clear();
    }

    std::sort(blocks.begin(), blocks.end(), [](const auto& a, const auto& b) {
        return a->getTxPoW().getBlockNumber().isLess(b->getTxPoW().getBlockNumber());
    });

    std::filesystem::path raw = zRawGzipOut;
    raw += ".tmp";
    {
        std::ofstream out(raw, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + raw.string());
        MiniByte::WriteToStream(out, cascade != nullptr);
        if (cascade) cascade->writeDataStream(out);
        MiniNumber(static_cast<int>(blocks.size())).writeDataStream(out);
        for (const auto& b : blocks) b->writeDataStream(out);
        out.close();
    }
    org::minima::utils::MiniFile::compressGzipFile(raw, zRawGzipOut);
    std::filesystem::remove(raw, ec);

    result.blocks = static_cast<int>(blocks.size());
    result.cascade = (cascade != nullptr);
    if (!blocks.empty()) {
        result.firstBlock = blocks.front()->getTxPoW().getBlockNumber().getAsLong();
        result.lastBlock = blocks.back()->getTxPoW().getBlockNumber().getAsLong();
    }
    return result;
}

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org
