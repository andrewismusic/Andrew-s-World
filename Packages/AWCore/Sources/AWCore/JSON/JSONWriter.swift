import Foundation

/// Writes `JSONValue` exactly as JavaScript's `JSON.stringify(value)` would: no whitespace, keys in order,
/// numbers in ECMAScript `Number::toString` form, and the same string escaping.
public enum JSONWriter {
    public static func data(_ value: JSONValue) -> Data {
        var out = [UInt8]()
        out.reserveCapacity(1 << 16)
        write(value, into: &out)
        return Data(out)
    }

    public static func string(_ value: JSONValue) -> String {
        String(decoding: data(value), as: UTF8.self)
    }

    static func write(_ v: JSONValue, into out: inout [UInt8]) {
        switch v {
        case .null: out.append(contentsOf: Array("null".utf8))
        case .bool(let b): out.append(contentsOf: Array((b ? "true" : "false").utf8))
        case .number(let n): out.append(contentsOf: Array(formatNumber(n).utf8))
        case .string(let s): writeString(s, into: &out)
        case .array(let a):
            out.append(UInt8(ascii: "["))
            for (j, x) in a.enumerated() {
                if j > 0 { out.append(UInt8(ascii: ",")) }
                write(x, into: &out)
            }
            out.append(UInt8(ascii: "]"))
        case .object(let o):
            out.append(UInt8(ascii: "{"))
            for (j, (k, x)) in o.pairs.enumerated() {
                if j > 0 { out.append(UInt8(ascii: ",")) }
                writeString(k, into: &out)
                out.append(UInt8(ascii: ":"))
                write(x, into: &out)
            }
            out.append(UInt8(ascii: "}"))
        }
    }

    private static let hex = Array("0123456789abcdef".utf8)

    /// `JSON.stringify` escapes quote, backslash and control characters; everything else, `/` included, is literal.
    static func writeString(_ s: String, into out: inout [UInt8]) {
        out.append(UInt8(ascii: "\""))
        for b in s.utf8 {
            switch b {
            case 0x22: out.append(contentsOf: [0x5C, 0x22])
            case 0x5C: out.append(contentsOf: [0x5C, 0x5C])
            case 0x08: out.append(contentsOf: [0x5C, UInt8(ascii: "b")])
            case 0x09: out.append(contentsOf: [0x5C, UInt8(ascii: "t")])
            case 0x0A: out.append(contentsOf: [0x5C, UInt8(ascii: "n")])
            case 0x0C: out.append(contentsOf: [0x5C, UInt8(ascii: "f")])
            case 0x0D: out.append(contentsOf: [0x5C, UInt8(ascii: "r")])
            case 0x00..<0x20:
                out.append(contentsOf: Array("\\u00".utf8))
                out.append(hex[Int(b >> 4)])
                out.append(hex[Int(b & 0xF)])
            default: out.append(b)
            }
        }
        out.append(UInt8(ascii: "\""))
    }

    /// ECMAScript Number::toString for finite doubles; NaN and infinities become null, as in `JSON.stringify`.
    /// Swift's `description` already gives the shortest digits that round-trip, which is what JavaScript uses;
    /// only the layout (when to use exponents, how to write them) differs, so we re-lay the digits out.
    public static func formatNumber(_ x: Double) -> String {
        guard x.isFinite else { return "null" }
        if x == 0 { return "0" }  // also -0, which JSON.stringify writes as 0
        if x == x.rounded(), abs(x) < 1e15 { return String(Int64(x)) }

        var s = x.description
        let negative = s.hasPrefix("-")
        if negative { s.removeFirst() }
        var exp = 0
        if let e = s.firstIndex(where: { $0 == "e" || $0 == "E" }) {
            exp = Int(s[s.index(after: e)...]) ?? 0
            s = String(s[..<e])
        }
        let parts = s.split(separator: ".", omittingEmptySubsequences: false)
        let intPart = String(parts[0])
        let fracPart = parts.count > 1 ? String(parts[1]) : ""
        var digits = intPart + fracPart
        // value = 0.d1d2d3… × 10^n
        var n = intPart.count + exp
        let leading = digits.prefix { $0 == "0" }.count
        digits.removeFirst(leading)
        n -= leading
        while digits.last == "0" { digits.removeLast() }
        if digits.isEmpty { return "0" }
        let k = digits.count

        var out: String
        if k <= n && n <= 21 {
            out = digits + String(repeating: "0", count: n - k)
        } else if 0 < n && n <= 21 {
            let cut = digits.index(digits.startIndex, offsetBy: n)
            out = String(digits[..<cut]) + "." + String(digits[cut...])
        } else if -6 < n && n <= 0 {
            out = "0." + String(repeating: "0", count: -n) + digits
        } else {
            let e = n - 1
            let first = String(digits.prefix(1))
            let rest = String(digits.dropFirst())
            out = first + (rest.isEmpty ? "" : "." + rest) + "e" + (e >= 0 ? "+" : "-") + String(abs(e))
        }
        return negative ? "-" + out : out
    }
}
