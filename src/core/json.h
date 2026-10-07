// Minimal recursive-descent JSON reader.
//
// Deliberately vendored rather than fetched: Lunar Trail must configure and
// build offline with only SDL2 present, and the content files under content/
// are simple enough that a few hundred lines covers them.
//
// Supports the JSON subset the game needs: objects, arrays, strings (with the
// standard escapes), numbers, true/false/null. Sufficient for config data;
// not intended as a general-purpose library.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lt {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Type::Null) {}

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool fallback = false) const { return isBool() ? bool_ : fallback; }
    double asNumber(double fallback = 0.0) const { return isNumber() ? num_ : fallback; }
    float asFloat(float fallback = 0.0f) const {
        return isNumber() ? static_cast<float>(num_) : fallback;
    }
    // Truncating conversion, made explicit. Balance fields mix doubles and ints,
    // so the narrowing has to be visible at the call site rather than implicit.
    int asInt(int fallback = 0) const {
        return isNumber() ? static_cast<int>(num_) : fallback;
    }
    const std::string& asString() const { return str_; }
    std::string asString(const std::string& fallback) const {
        return isString() ? str_ : fallback;
    }

    size_t size() const { return isArray() ? arr_.size() : obj_.size(); }

    const Json& operator[](size_t i) const {
        static const Json kNull;
        return isArray() && i < arr_.size() ? arr_[i] : kNull;
    }
    const Json& operator[](const std::string& key) const {
        static const Json kNull;
        if (!isObject()) return kNull;
        auto it = obj_.find(key);
        return it == obj_.end() ? kNull : it->second;
    }
    bool has(const std::string& key) const { return isObject() && obj_.count(key) != 0; }

    // Iteration over arrays.
    const std::vector<Json>& items() const { return arr_; }

    // Typed lookups with fallbacks, so content files can omit keys safely.
    double num(const std::string& key, double fallback) const {
        return has(key) ? (*this)[key].asNumber(fallback) : fallback;
    }
    int integer(const std::string& key, int fallback) const {
        return has(key) ? (*this)[key].asInt(fallback) : fallback;
    }
    bool boolean(const std::string& key, bool fallback) const {
        return has(key) ? (*this)[key].asBool(fallback) : fallback;
    }
    std::string text(const std::string& key, const std::string& fallback) const {
        return has(key) ? (*this)[key].asString(fallback) : fallback;
    }

    // Parses `text`. On failure returns a Null value and, if `error` is
    // non-null, fills it with a human-readable message including the byte
    // offset.
    static Json parse(const std::string& text, std::string* error = nullptr);
    static Json parseFile(const std::string& path, std::string* error = nullptr);

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> arr_;
    std::map<std::string, Json> obj_;

    friend class JsonParser;
};

}  // namespace lt
