#include "core/json.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace lt {

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : s_(src) {}

    Json parse() {
        skipWs();
        Json v = parseValue();
        if (!err_.empty()) return Json();
        skipWs();
        if (pos_ != s_.size()) {
            fail("trailing characters after top-level value");
            return Json();
        }
        return v;
    }

    const std::string& error() const { return err_; }

private:
    const std::string& s_;
    size_t pos_ = 0;
    std::string err_;

    void fail(const std::string& msg) {
        if (!err_.empty()) return;
        std::ostringstream os;
        os << "JSON parse error at byte " << pos_ << ": " << msg;
        err_ = os.str();
    }

    bool eof() const { return pos_ >= s_.size(); }
    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }

    void skipWs() {
        while (pos_ < s_.size()) {
            const char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool literal(const char* lit) {
        const size_t n = std::char_traits<char>::length(lit);
        if (s_.compare(pos_, n, lit) != 0) return false;
        pos_ += n;
        return true;
    }

    Json parseValue() {
        if (eof()) {
            fail("unexpected end of input");
            return Json();
        }
        switch (peek()) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': return parseString();
            case 't':
                if (literal("true")) { Json j; j.type_ = Json::Type::Bool; j.bool_ = true; return j; }
                fail("bad literal");
                return Json();
            case 'f':
                if (literal("false")) { Json j; j.type_ = Json::Type::Bool; j.bool_ = false; return j; }
                fail("bad literal");
                return Json();
            case 'n':
                if (literal("null")) return Json();
                fail("bad literal");
                return Json();
            default: return parseNumber();
        }
    }

    Json parseObject() {
        Json j;
        j.type_ = Json::Type::Object;
        ++pos_;  // '{'
        skipWs();
        if (peek() == '}') { ++pos_; return j; }
        for (;;) {
            skipWs();
            if (peek() != '"') { fail("expected object key string"); return Json(); }
            Json key = parseString();
            if (!err_.empty()) return Json();
            skipWs();
            if (peek() != ':') { fail("expected ':' after object key"); return Json(); }
            ++pos_;
            skipWs();
            Json val = parseValue();
            if (!err_.empty()) return Json();
            j.obj_[key.str_] = std::move(val);
            skipWs();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; return j; }
            fail("expected ',' or '}' in object");
            return Json();
        }
    }

    Json parseArray() {
        Json j;
        j.type_ = Json::Type::Array;
        ++pos_;  // '['
        skipWs();
        if (peek() == ']') { ++pos_; return j; }
        for (;;) {
            skipWs();
            Json val = parseValue();
            if (!err_.empty()) return Json();
            j.arr_.push_back(std::move(val));
            skipWs();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; return j; }
            fail("expected ',' or ']' in array");
            return Json();
        }
    }

    void appendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
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

    uint32_t parseHex4() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            if (eof()) return 0;
            const char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9')      v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else { fail("bad \\u escape"); return 0; }
        }
        return v;
    }

    Json parseString() {
        Json j;
        j.type_ = Json::Type::String;
        ++pos_;  // opening quote
        std::string out;
        for (;;) {
            if (eof()) { fail("unterminated string"); return Json(); }
            const char c = s_[pos_++];
            if (c == '"') break;
            if (c != '\\') { out.push_back(c); continue; }
            if (eof()) { fail("unterminated escape"); return Json(); }
            const char e = s_[pos_++];
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = parseHex4();
                    if (!err_.empty()) return Json();
                    // Surrogate pair.
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 1 < s_.size() &&
                        s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        const uint32_t lo = parseHex4();
                        if (!err_.empty()) return Json();
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            appendUtf8(out, cp);
                            cp = lo;
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: fail("unknown escape sequence"); return Json();
            }
        }
        j.str_ = std::move(out);
        return j;
    }

    Json parseNumber() {
        const size_t start = pos_;
        if (peek() == '-' || peek() == '+') ++pos_;
        while (!eof()) {
            const char c = s_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        if (pos_ == start) { fail("expected a value"); return Json(); }
        const std::string tok = s_.substr(start, pos_ - start);
        char* end = nullptr;
        const double v = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str()) { fail("malformed number"); return Json(); }
        Json j;
        j.type_ = Json::Type::Number;
        j.num_ = v;
        return j;
    }
};

Json Json::parse(const std::string& text, std::string* error) {
    JsonParser p(text);
    Json v = p.parse();
    if (error) *error = p.error();
    return v;
}

Json Json::parseFile(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "cannot open " + path;
        return Json();
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    std::string inner;
    Json v = parse(text, &inner);
    if (error != nullptr) {
        if (!inner.empty()) {
            *error = path + ": " + inner;
        } else {
            error->clear();
        }
    }
    return v;
}

}  // namespace lt
