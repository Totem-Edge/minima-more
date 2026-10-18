#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>

#include "org/minima/utils/json/parser/yylex.hpp"
#include "org/minima/utils/json/parser/yytoken.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"
#include "org/minima/utils/json/parser/container_factory.hpp"
#include "org/minima/utils/json/parser/content_handler.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

// Local helper: convert Yytoken::Value (variant) to std::any for JSONObject/JSONArray and ContentHandler.
static std::any tokenValueToAny(const Yytoken::Value& v) {
    // variant alternatives: std::monostate, std::nullptr_t, bool, long long, double, std::string
    if (std::holds_alternative<std::monostate>(v)) {
        return std::any(); // equivalent to "no value"
    }
    if (std::holds_alternative<std::nullptr_t>(v)) {
        return std::any(nullptr);
    }
    if (std::holds_alternative<bool>(v)) {
        return std::any(std::get<bool>(v));
    }
    if (std::holds_alternative<long long>(v)) {
        return std::any(std::get<long long>(v));
    }
    if (std::holds_alternative<double>(v)) {
        return std::any(std::get<double>(v));
    }
    if (std::holds_alternative<std::string>(v)) {
        return std::any(std::get<std::string>(v));
    }
    // Fallback (shouldn't happen)
    return std::any();
}

// Implement YylexDeleter now that Yylex is a complete type.
void JSONParser::YylexDeleter::operator()(Yylex* p) const {
    delete p;
}

JSONParser::JSONParser()
    : m_lexer(nullptr)
    , m_token(nullptr)
    , m_status(S_INIT)
    , m_handlerStatusStack(nullptr) {
}

JSONParser::~JSONParser() = default;

void JSONParser::reset() {
    m_token.reset();
    m_status = S_INIT;
    m_handlerStatusStack.reset();
}

void JSONParser::reset(std::istream& in) {
    if (!m_lexer) {
        m_lexer.reset(new Yylex(in));
    } else {
        m_lexer->yyreset(in);
    }
    reset();
}

int JSONParser::getPosition() const {
    return m_lexer ? m_lexer->getPosition() : -1;
}

std::any JSONParser::parse(const std::string& s) {
    return parse(s, static_cast<ContainerFactory*>(nullptr));
}

std::any JSONParser::parse(const std::string& s, ContainerFactory* containerFactory) {
    std::istringstream in(s);
    try {
        return parse(in, containerFactory);
    } catch (const std::ios_base::failure& ie) {
        // "Actually it will never happen."
        throw ParseException(-1, ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string(ie.what()));
    }
}

std::any JSONParser::parse(std::istream& in) {
    return parse(in, static_cast<ContainerFactory*>(nullptr));
}

std::any JSONParser::parse(std::istream& in, ContainerFactory* containerFactory) {
    reset(in);
    std::deque<int> statusStack;
    std::deque<std::any> valueStack;

    try {
        do {
            nextToken();
            switch (m_status) {
                case S_INIT: {
                    if (m_token->type == Yytoken::TYPE_EOF) {
                        return std::any(); 
                    }
                    switch (m_token->type) {
                        case Yytoken::TYPE_VALUE: {
                            m_status = S_IN_FINISHED_VALUE;
                            statusStack.push_front(m_status);
                            valueStack.push_front(tokenValueToAny(m_token->value));
                            break;
                        }
                        case Yytoken::TYPE_LEFT_BRACE: {
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            auto obj = createObjectContainer(containerFactory);
                            valueStack.push_front(obj);
                            break;
                        }
                        case Yytoken::TYPE_LEFT_SQUARE: {
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            auto arr = createArrayContainer(containerFactory);
                            valueStack.push_front(arr);
                            break;
                        }
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_IN_FINISHED_VALUE: {
                    if (m_token->type == Yytoken::TYPE_EOF) {
                        std::any ret = valueStack.front();
                        valueStack.pop_front();
                        return ret;
                    } else {
                        throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
                    }
                }

                case S_IN_OBJECT: {
                    switch (m_token->type) {
                        case Yytoken::TYPE_COMMA:
                            break;
                        case Yytoken::TYPE_VALUE:
                            if (std::holds_alternative<std::string>(m_token->value)) {
                                std::string key = std::get<std::string>(m_token->value);
                                valueStack.push_front(key);
                                m_status = S_PASSED_PAIR_KEY;
                                statusStack.push_front(m_status);
                            } else {
                                m_status = S_IN_ERROR;
                            }
                            break;
                        case Yytoken::TYPE_RIGHT_BRACE:
                            if (valueStack.size() > 1) {
                                if (!statusStack.empty()) statusStack.pop_front();
                                if (!valueStack.empty()) valueStack.pop_front();
                                m_status = peekStatus(statusStack);
                            } else {
                                m_status = S_IN_FINISHED_VALUE;
                            }
                            break;
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_PASSED_PAIR_KEY: {
                    switch (m_token->type) {
                        case Yytoken::TYPE_COLON:
                            break;
                        case Yytoken::TYPE_VALUE: {
                            if (!statusStack.empty()) statusStack.pop_front();
                            std::string key = std::any_cast<std::string>(valueStack.front());
                            valueStack.pop_front();
                            auto parent = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONObject>>(valueStack.front());
                            parent->put(key, tokenValueToAny(m_token->value));
                            m_status = peekStatus(statusStack);
                            break;
                        }
                        case Yytoken::TYPE_LEFT_SQUARE: {
                            if (!statusStack.empty()) statusStack.pop_front();
                            std::string key = std::any_cast<std::string>(valueStack.front());
                            valueStack.pop_front();
                            auto parent = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONObject>>(valueStack.front());
                            auto newArray = createArrayContainer(containerFactory);
                            parent->put(key, std::any(newArray));
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            valueStack.push_front(newArray);
                            break;
                        }
                        case Yytoken::TYPE_LEFT_BRACE: {
                            if (!statusStack.empty()) statusStack.pop_front();
                            std::string key = std::any_cast<std::string>(valueStack.front());
                            valueStack.pop_front();
                            auto parent = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONObject>>(valueStack.front());
                            auto newObject = createObjectContainer(containerFactory);
                            parent->put(key, std::any(newObject));
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            valueStack.push_front(newObject);
                            break;
                        }
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_IN_ARRAY: {
                    switch (m_token->type) {
                        case Yytoken::TYPE_COMMA:
                            break;
                        case Yytoken::TYPE_VALUE: {
                            auto arr = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONArray>>(valueStack.front());
                            arr->add(tokenValueToAny(m_token->value));
                            break;
                        }
                        case Yytoken::TYPE_RIGHT_SQUARE:
                            if (valueStack.size() > 1) {
                                if (!statusStack.empty()) statusStack.pop_front();
                                if (!valueStack.empty()) valueStack.pop_front();
                                m_status = peekStatus(statusStack);
                            } else {
                                m_status = S_IN_FINISHED_VALUE;
                            }
                            break;
                        case Yytoken::TYPE_LEFT_BRACE: {
                            auto arr = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONArray>>(valueStack.front());
                            auto newObject = createObjectContainer(containerFactory);
                            arr->add(std::any(newObject));
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            valueStack.push_front(newObject);
                            break;
                        }
                        case Yytoken::TYPE_LEFT_SQUARE: {
                            auto arr = std::any_cast<std::shared_ptr<org::minima::utils::json::JSONArray>>(valueStack.front());
                            auto newArray = createArrayContainer(containerFactory);
                            arr->add(std::any(newArray));
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            valueStack.push_front(newArray);
                            break;
                        }
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_IN_ERROR:
                    throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);

                default:
                    break;
            }

            if (m_status == S_IN_ERROR) {
                throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
            }
        } while (m_token->type != Yytoken::TYPE_EOF);
    } catch (const std::ios_base::failure&) {
        throw; // rethrow IO errors unchanged
    }

    throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
}

void JSONParser::parse(const std::string& s, ContentHandler& contentHandler) {
    parse(s, contentHandler, false);
}

void JSONParser::parse(const std::string& s, ContentHandler& contentHandler, bool isResume) {
    std::istringstream in(s);
    try {
        parse(in, contentHandler, isResume);
    } catch (const std::ios_base::failure& ie) {
        throw ParseException(-1, ParseException::ERROR_UNEXPECTED_EXCEPTION, std::string(ie.what()));
    }
}

void JSONParser::parse(std::istream& in, ContentHandler& contentHandler) {
    parse(in, contentHandler, false);
}

void JSONParser::parse(std::istream& in, ContentHandler& contentHandler, bool isResume) {
    if (!isResume) {
        reset(in);
        m_handlerStatusStack = std::make_unique<std::deque<int>>();
    } else {
        if (!m_handlerStatusStack) {
            isResume = false;
            reset(in);
            m_handlerStatusStack = std::make_unique<std::deque<int>>();
        }
    }

    std::deque<int>& statusStack = *m_handlerStatusStack;

    try {
        do {
            switch (m_status) {
                case S_INIT: {
                    contentHandler.startJSON();
                    nextToken();
                    if (m_token->type == Yytoken::TYPE_EOF) {
                        contentHandler.endJSON();
                        m_status = S_END;
                        return;
                    }
                    switch (m_token->type) {
                        case Yytoken::TYPE_VALUE:
                            m_status = S_IN_FINISHED_VALUE;
                            statusStack.push_front(m_status);
                            if (!contentHandler.primitive(tokenValueToAny(m_token->value))) return;
                            break;
                        case Yytoken::TYPE_LEFT_BRACE:
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startObject()) return;
                            break;
                        case Yytoken::TYPE_LEFT_SQUARE:
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startArray()) return;
                            break;
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_IN_FINISHED_VALUE: {
                    nextToken();
                    if (m_token->type == Yytoken::TYPE_EOF) {
                        contentHandler.endJSON();
                        m_status = S_END;
                        return;
                    } else {
                        m_status = S_IN_ERROR;
                        throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
                    }
                }

                case S_IN_OBJECT: {
                    nextToken();
                    switch (m_token->type) {
                        case Yytoken::TYPE_COMMA:
                            break;
                        case Yytoken::TYPE_VALUE:
                            if (std::holds_alternative<std::string>(m_token->value)) {
                                std::string key = std::get<std::string>(m_token->value);
                                m_status = S_PASSED_PAIR_KEY;
                                statusStack.push_front(m_status);
                                if (!contentHandler.startObjectEntry(key)) return;
                            } else {
                                m_status = S_IN_ERROR;
                            }
                            break;
                        case Yytoken::TYPE_RIGHT_BRACE:
                            if (statusStack.size() > 1) {
                                statusStack.pop_front();
                                m_status = peekStatus(statusStack);
                            } else {
                                m_status = S_IN_FINISHED_VALUE;
                            }
                            if (!contentHandler.endObject()) return;
                            break;
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_PASSED_PAIR_KEY: {
                    nextToken();
                    switch (m_token->type) {
                        case Yytoken::TYPE_COLON:
                            break;
                        case Yytoken::TYPE_VALUE:
                            statusStack.pop_front();
                            m_status = peekStatus(statusStack);
                            if (!contentHandler.primitive(tokenValueToAny(m_token->value))) return;
                            if (!contentHandler.endObjectEntry()) return;
                            break;
                        case Yytoken::TYPE_LEFT_SQUARE:
                            statusStack.pop_front();
                            statusStack.push_front(S_IN_PAIR_VALUE);
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startArray()) return;
                            break;
                        case Yytoken::TYPE_LEFT_BRACE:
                            statusStack.pop_front();
                            statusStack.push_front(S_IN_PAIR_VALUE);
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startObject()) return;
                            break;
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_IN_PAIR_VALUE: {
                    // Marker state; does not consume token here.
                    statusStack.pop_front();
                    m_status = peekStatus(statusStack);
                    if (!contentHandler.endObjectEntry()) return;
                    break;
                }

                case S_IN_ARRAY: {
                    nextToken();
                    switch (m_token->type) {
                        case Yytoken::TYPE_COMMA:
                            break;
                        case Yytoken::TYPE_VALUE:
                            if (!contentHandler.primitive(tokenValueToAny(m_token->value))) return;
                            break;
                        case Yytoken::TYPE_RIGHT_SQUARE:
                            if (statusStack.size() > 1) {
                                statusStack.pop_front();
                                m_status = peekStatus(statusStack);
                            } else {
                                m_status = S_IN_FINISHED_VALUE;
                            }
                            if (!contentHandler.endArray()) return;
                            break;
                        case Yytoken::TYPE_LEFT_BRACE:
                            m_status = S_IN_OBJECT;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startObject()) return;
                            break;
                        case Yytoken::TYPE_LEFT_SQUARE:
                            m_status = S_IN_ARRAY;
                            statusStack.push_front(m_status);
                            if (!contentHandler.startArray()) return;
                            break;
                        default:
                            m_status = S_IN_ERROR;
                            break;
                    }
                    break;
                }

                case S_END:
                    return;

                case S_IN_ERROR:
                    throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);

                default:
                    break;
            }

            if (m_status == S_IN_ERROR) {
                throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
            }
        } while (m_token->type != Yytoken::TYPE_EOF);
    } catch (const std::ios_base::failure&) {
        m_status = S_IN_ERROR;
        throw;
    } catch (const ParseException&) {
        m_status = S_IN_ERROR;
        throw;
    } catch (const std::exception&) {
        m_status = S_IN_ERROR;
        throw;
    } catch (...) {
        m_status = S_IN_ERROR;
        throw;
    }

    m_status = S_IN_ERROR;
    throw ParseException(getPosition(), ParseException::ERROR_UNEXPECTED_TOKEN, *m_token);
}

void JSONParser::nextToken() {
    // Acquire next token from lexer; if null, synthesize EOF token.
    std::unique_ptr<Yytoken> t = m_lexer ? m_lexer->yylex() : std::unique_ptr<Yytoken>();
    if (!t) {
        // Construct an EOF token.
        m_token.reset(new Yytoken(Yytoken::TYPE_EOF));
    } else {
        m_token = std::move(t);
    }
}

int JSONParser::peekStatus(const std::deque<int>& statusStack) {
    if (statusStack.empty()) return -1;
    return statusStack.front();
}

// Helper to alias-cast shared_ptr<void> returned by ContainerFactory into typed shared_ptr<T>.
// Shares ownership and assumes the underlying object is actually of type T (or derived).
template<typename T>
static std::shared_ptr<T> alias_cast_shared(const std::shared_ptr<void>& p) {
    if (!p) return nullptr;
    return std::shared_ptr<T>(p, reinterpret_cast<T*>(p.get()));
}

std::shared_ptr<org::minima::utils::json::JSONObject>
JSONParser::createObjectContainer(ContainerFactory* containerFactory) {
    using org::minima::utils::json::JSONObject;
    if (containerFactory == nullptr) {
        return std::make_shared<JSONObject>();
    }
    std::shared_ptr<void> m = containerFactory->createObjectContainer();
    if (!m) {
        return std::make_shared<JSONObject>();
    }
    return alias_cast_shared<JSONObject>(m);
}

std::shared_ptr<org::minima::utils::json::JSONArray>
JSONParser::createArrayContainer(ContainerFactory* containerFactory) {
    using org::minima::utils::json::JSONArray;
    if (containerFactory == nullptr) {
        return std::make_shared<JSONArray>();
    }
    std::shared_ptr<void> l = containerFactory->creatArrayContainer(); // matches Java method name
    if (!l) {
        return std::make_shared<JSONArray>();
    }
    return alias_cast_shared<JSONArray>(l);
}

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org