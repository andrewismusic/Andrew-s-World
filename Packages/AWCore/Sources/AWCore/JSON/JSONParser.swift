import Foundation

public struct JSONParseError: Error, CustomStringConvertible, Sendable {
    public let message: String
    public let offset: Int
    public var description: String { "JSON parse error at byte \(offset): \(message)" }
}

/// Parses JSON into `JSONValue`, keeping object key order. Matches `JSON.parse`: a repeated key keeps its first
/// position and takes the last value; leading and trailing whitespace is allowed.
public struct JSONParser {
    private let bytes: [UInt8]
    private var i = 0

    public static func parse(_ data: Data) throws -> JSONValue {
        var p = JSONParser(bytes: [UInt8](data))
        p.skipWhitespace()
        let v = try p.parseValue(depth: 0)
        p.skipWhitespace()
        guard p.i == p.bytes.count else { throw p.error("unexpected trailing characters") }
        return v
    }

    public static func parse(_ string: String) throws -> JSONValue {
        try parse(Data(string.utf8))
    }

    private init(bytes: [UInt8]) {
        // a UTF-8 byte order mark is not JSON, but files saved by some editors start with one
        if bytes.starts(with: [0xEF, 0xBB, 0xBF]) { self.bytes = Array(bytes.dropFirst(3)) } else { self.bytes = bytes }
    }

    private func error(_ m: String) -> JSONParseError { JSONParseError(message: m, offset: i) }

    private mutating func skipWhitespace() {
        while i < bytes.count, [0x20, 0x09, 0x0A, 0x0D].contains(bytes[i]) { i += 1 }
    }

    private mutating func parseValue(depth: Int) throws -> JSONValue {
        guard depth < 512 else { throw error("nested too deeply") }
        guard i < bytes.count else { throw error("unexpected end of input") }
        switch bytes[i] {
        case UInt8(ascii: "{"): return try parseObject(depth: depth)
        case UInt8(ascii: "["): return try parseArray(depth: depth)
        case UInt8(ascii: "\""): return .string(try parseString())
        case UInt8(ascii: "t"): try expect("true"); return .bool(true)
        case UInt8(ascii: "f"): try expect("false"); return .bool(false)
        case UInt8(ascii: "n"): try expect("null"); return .null
        default: return .number(try parseNumber())
        }
    }

    private mutating func expect(_ word: String) throws {
        let w = Array(word.utf8)
        guard i + w.count <= bytes.count, Array(bytes[i..<i + w.count]) == w else { throw error("expected \(word)") }
        i += w.count
    }

    private mutating func parseObject(depth: Int) throws -> JSONValue {
        i += 1
        var o = JSONObject()
        skipWhitespace()
        if i < bytes.count, bytes[i] == UInt8(ascii: "}") { i += 1; return .object(o) }
        while true {
            skipWhitespace()
            guard i < bytes.count, bytes[i] == UInt8(ascii: "\"") else { throw error("expected a key") }
            let k = try parseString()
            skipWhitespace()
            guard i < bytes.count, bytes[i] == UInt8(ascii: ":") else { throw error("expected ':'") }
            i += 1
            skipWhitespace()
            o[k] = try parseValue(depth: depth + 1)
            skipWhitespace()
            guard i < bytes.count else { throw error("unterminated object") }
            if bytes[i] == UInt8(ascii: ",") { i += 1; continue }
            if bytes[i] == UInt8(ascii: "}") { i += 1; return .object(o) }
            throw error("expected ',' or '}'")
        }
    }

    private mutating func parseArray(depth: Int) throws -> JSONValue {
        i += 1
        var a: [JSONValue] = []
        skipWhitespace()
        if i < bytes.count, bytes[i] == UInt8(ascii: "]") { i += 1; return .array(a) }
        while true {
            skipWhitespace()
            a.append(try parseValue(depth: depth + 1))
            skipWhitespace()
            guard i < bytes.count else { throw error("unterminated array") }
            if bytes[i] == UInt8(ascii: ",") { i += 1; continue }
            if bytes[i] == UInt8(ascii: "]") { i += 1; return .array(a) }
            throw error("expected ',' or ']'")
        }
    }

    private mutating func parseNumber() throws -> Double {
        let start = i
        if i < bytes.count, bytes[i] == UInt8(ascii: "-") { i += 1 }
        func digits() -> Int { let s = i; while i < bytes.count, bytes[i] >= 0x30, bytes[i] <= 0x39 { i += 1 }; return i - s }
        guard i < bytes.count else { throw error("expected a value") }
        if bytes[i] == UInt8(ascii: "0") { i += 1 } else if digits() == 0 { throw error("expected a value") }
        if i < bytes.count, bytes[i] == UInt8(ascii: ".") {
            i += 1
            guard digits() > 0 else { throw error("expected digits after '.'") }
        }
        if i < bytes.count, bytes[i] == UInt8(ascii: "e") || bytes[i] == UInt8(ascii: "E") {
            i += 1
            if i < bytes.count, bytes[i] == UInt8(ascii: "+") || bytes[i] == UInt8(ascii: "-") { i += 1 }
            guard digits() > 0 else { throw error("expected exponent digits") }
        }
        let text = String(decoding: bytes[start..<i], as: UTF8.self)
        guard let d = Double(text) else { throw error("bad number \(text)") }
        return d
    }

    private mutating func parseHex4() throws -> UInt16 {
        guard i + 4 <= bytes.count else { throw error("short \\u escape") }
        var v: UInt16 = 0
        for _ in 0..<4 {
            let c = bytes[i]
            let d: UInt8
            switch c {
            case 0x30...0x39: d = c - 0x30
            case 0x41...0x46: d = c - 0x41 + 10
            case 0x61...0x66: d = c - 0x61 + 10
            default: throw error("bad \\u escape")
            }
            v = v << 4 | UInt16(d)
            i += 1
        }
        return v
    }

    private mutating func parseString() throws -> String {
        i += 1
        var out = [UInt8]()
        while true {
            guard i < bytes.count else { throw error("unterminated string") }
            let c = bytes[i]
            if c == UInt8(ascii: "\"") { i += 1; break }
            if c < 0x20 { throw error("control character in string") }
            if c != UInt8(ascii: "\\") { out.append(c); i += 1; continue }
            i += 1
            guard i < bytes.count else { throw error("unterminated escape") }
            let e = bytes[i]
            i += 1
            switch e {
            case UInt8(ascii: "\""): out.append(0x22)
            case UInt8(ascii: "\\"): out.append(0x5C)
            case UInt8(ascii: "/"): out.append(0x2F)
            case UInt8(ascii: "b"): out.append(0x08)
            case UInt8(ascii: "f"): out.append(0x0C)
            case UInt8(ascii: "n"): out.append(0x0A)
            case UInt8(ascii: "r"): out.append(0x0D)
            case UInt8(ascii: "t"): out.append(0x09)
            case UInt8(ascii: "u"):
                var units = [try parseHex4()]
                // a high surrogate followed by an escaped low surrogate is one character
                if (0xD800...0xDBFF).contains(units[0]), i + 6 <= bytes.count,
                   bytes[i] == UInt8(ascii: "\\"), bytes[i + 1] == UInt8(ascii: "u") {
                    let save = i
                    i += 2
                    let lo = try parseHex4()
                    if (0xDC00...0xDFFF).contains(lo) { units.append(lo) } else { i = save }
                }
                // lone surrogates can't live in a Swift String; they become U+FFFD
                out.append(contentsOf: Array(String(decoding: units, as: UTF16.self).utf8))
            default: throw error("bad escape")
            }
        }
        return String(decoding: out, as: UTF8.self)
    }
}
