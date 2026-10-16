import Foundation

/// A flat map of lock keys to normalised 0–1 values, e.g. `"1.bass.cf": 0.62`, `"rat.rt": 0.4`, `"g.bpm": 0.47`.
///
/// Keys are `"<slot>.<engine>.<param>"` for track sound, or a global prefix: `rat.*` (note), `g.*` (score),
/// `space.*` (song), `master.*`, `<slot>.fx<i>.<building>.<param>` (effects). Order is kept, like the
/// JavaScript object it mirrors, so saves round-trip exactly.
public struct LockMap: Equatable, Sendable, Sequence, ExpressibleByDictionaryLiteral {
    public private(set) var keys: [String] = []
    private var values: [String: Double] = [:]

    public init() {}

    public init(dictionaryLiteral elements: (String, Double)...) {
        for (k, v) in elements { self[k] = v }
    }

    public var count: Int { keys.count }
    public var isEmpty: Bool { keys.isEmpty }

    /// Setting an existing key keeps its position, a new key goes last, and nil removes the lock.
    public subscript(key: String) -> Double? {
        get { values[key] }
        set {
            if let newValue {
                if values.updateValue(newValue, forKey: key) == nil { keys.append(key) }
            } else if values.removeValue(forKey: key) != nil {
                keys.removeAll { $0 == key }
            }
        }
    }

    public func makeIterator() -> AnyIterator<(key: String, value: Double)> {
        var i = 0
        return AnyIterator {
            guard i < keys.count else { return nil }
            defer { i += 1 }
            return (keys[i], values[keys[i]]!)
        }
    }

    public mutating func removeAll() {
        keys.removeAll()
        values.removeAll()
    }

    public static func == (a: LockMap, b: LockMap) -> Bool {
        // NaN locks (written as null by the prototype) compare equal to each other
        a.keys == b.keys && a.keys.allSatisfy { k in
            let x = a.values[k]!, y = b.values[k]!
            return x == y || (x.isNaN && y.isNaN)
        }
    }
}

extension LockMap: Codable {
    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        for k in c.allKeys {
            // null comes from a NaN in the prototype; keep it so it writes back as null
            self[k.stringValue] = try c.decodeNil(forKey: k) ? .nan : c.decode(Double.self, forKey: k)
        }
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        for (k, v) in self { try c.encode(v, forKey: AnyCodingKey(k)) }
    }
}
