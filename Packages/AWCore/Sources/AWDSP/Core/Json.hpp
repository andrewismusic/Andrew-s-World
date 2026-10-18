// A small JSON reader for the save format. Runs off the audio thread only. Objects keep their key order.
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aw::json {

struct Member;

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::vector<Member> object;
    /// Where this value sits in the source text, so callers can cache by the exact text.
    size_t begin = 0, end = 0;

    bool isNull() const { return type == Type::Null; }
    bool isNumber() const { return type == Type::Number; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
    bool isString() const { return type == Type::String; }

    /// Object member, or nullptr.
    const Value *get(std::string_view key) const;
    double num(std::string_view key, double fallback) const;
    /// A number, with null read as 0 the way JavaScript arithmetic does.
    double asNumber(double fallback = 0) const;
    bool truthy() const;
};

struct Member {
    std::string first;
    Value second;
};

/// Parses text; returns false (and leaves `out` unspecified) on malformed input.
bool parse(std::string_view text, Value &out);

} // namespace aw::json
