#include "Json.hpp"

#include <cstdlib>
#include <cstring>

namespace aw::json {

const Value *Value::get(std::string_view key) const {
    if (type != Type::Object) return nullptr;
    for (auto &kv : object)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

double Value::num(std::string_view key, double fallback) const {
    const Value *v = get(key);
    return v ? v->asNumber(fallback) : fallback;
}

double Value::asNumber(double fallback) const {
    switch (type) {
    case Type::Number: return number;
    case Type::Null: return 0;
    case Type::Bool: return boolean ? 1 : 0;
    default: return fallback;
    }
}

bool Value::truthy() const {
    switch (type) {
    case Type::Null: return false;
    case Type::Bool: return boolean;
    case Type::Number: return number != 0 && number == number;
    case Type::String: return !string.empty();
    default: return true;
    }
}

namespace {

struct Parser {
    std::string_view s;
    size_t i = 0;
    int depth = 0;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }

    bool literal(const char *word) {
        size_t n = std::strlen(word);
        if (s.substr(i, n) != word) return false;
        i += n;
        return true;
    }

    static void utf8(std::string &out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned &v) {
        if (i + 4 > s.size()) return false;
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }

    bool str(std::string &out) {
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) return false;
            char e = s[i++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    size_t save = i;
                    i += 2;
                    unsigned lo;
                    if (hex4(lo) && lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    else i = save;
                }
                utf8(out, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool value(Value &v) {
        if (++depth > 64) return false;
        ws();
        if (i >= s.size()) return false;
        v.begin = i;
        char c = s[i];
        bool ok = true;
        if (c == '{') {
            v.type = Value::Type::Object;
            ++i;
            ws();
            if (i < s.size() && s[i] == '}') ++i;
            else
                for (;;) {
                    ws();
                    std::string key;
                    if (!str(key)) return false;
                    ws();
                    if (i >= s.size() || s[i] != ':') return false;
                    ++i;
                    v.object.push_back(Member{std::move(key), Value{}});
                    if (!value(v.object.back().second)) return false;
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; break; }
                    return false;
                }
        } else if (c == '[') {
            v.type = Value::Type::Array;
            ++i;
            ws();
            if (i < s.size() && s[i] == ']') ++i;
            else
                for (;;) {
                    v.array.emplace_back();
                    if (!value(v.array.back())) return false;
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; break; }
                    return false;
                }
        } else if (c == '"') {
            v.type = Value::Type::String;
            ok = str(v.string);
        } else if (c == 't') {
            v.type = Value::Type::Bool;
            v.boolean = true;
            ok = literal("true");
        } else if (c == 'f') {
            v.type = Value::Type::Bool;
            ok = literal("false");
        } else if (c == 'n') {
            ok = literal("null");
        } else {
            v.type = Value::Type::Number;
            size_t start = i;
            while (i < s.size() && (std::strchr("+-.eE", s[i]) || (s[i] >= '0' && s[i] <= '9'))) ++i;
            if (i == start) return false;
            std::string num(s.substr(start, i - start));
            char *endp = nullptr;
            v.number = std::strtod(num.c_str(), &endp);
            ok = endp && *endp == 0;
        }
        v.end = i;
        --depth;
        return ok;
    }
};

} // namespace

bool parse(std::string_view text, Value &out) {
    Parser p{text};
    out = Value{};
    if (!p.value(out)) return false;
    p.ws();
    return p.i == text.size();
}

} // namespace aw::json
