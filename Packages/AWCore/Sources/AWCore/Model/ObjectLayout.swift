import Foundation

/// How a JSON object in a save was laid out: the order its keys arrived in, and any keys this version of the
/// app doesn't know about. Every model type carries one so that loading and saving changes nothing.
///
/// JavaScript writes keys in the order they were first set, and that differs between objects of the same kind
/// (a song made by `newSong()` puts `fx` before `locks`; a demo song puts it last). New keys set after loading
/// go after the remembered ones, which is also what JavaScript does.
public struct ObjectLayout: Equatable, Sendable {
    public var keyOrder: [String] = []
    public var unknown: JSONObject = JSONObject()

    public init() {}

    /// The keys to write: remembered order first, then present keys in the type's usual order, then unknown keys.
    func orderedKeys(present: [String]) -> [String] {
        let presentSet = Set(present)
        var out: [String] = []
        var seen = Set<String>()
        for k in keyOrder where (presentSet.contains(k) || unknown[k] != nil) && seen.insert(k).inserted { out.append(k) }
        for k in present where seen.insert(k).inserted { out.append(k) }
        for k in unknown.keys where seen.insert(k).inserted { out.append(k) }
        return out
    }

    /// Forget where a key sat, so that if it is set again it goes last, as a deleted JavaScript property would.
    mutating func forget(_ key: String) {
        keyOrder.removeAll { $0 == key }
    }
}

// Shared plumbing for the model's hand-written `Codable` conformances.

extension KeyedDecodingContainer where K == AnyCodingKey {
    /// Records key order and keeps every key not in `known` as raw JSON.
    func readLayout(known: Set<String>) throws -> ObjectLayout {
        var l = ObjectLayout()
        l.keyOrder = allKeys.map(\.stringValue)
        for k in allKeys where !known.contains(k.stringValue) {
            l.unknown[k.stringValue] = try decode(JSONValue.self, forKey: k)
        }
        return l
    }

    func value<T: Decodable>(_ key: String, as type: T.Type = T.self) throws -> T {
        try decode(T.self, forKey: AnyCodingKey(key))
    }

    func optionalValue<T: Decodable>(_ key: String, as type: T.Type = T.self) throws -> T? {
        try decodeIfPresent(T.self, forKey: AnyCodingKey(key))
    }
}

extension KeyedEncodingContainer where K == AnyCodingKey {
    /// Writes keys in layout order. `write` handles the known keys; unknown ones are written back untouched.
    mutating func writeLayout(_ layout: ObjectLayout, present: [String], write: (String, inout Self) throws -> Void) throws {
        for k in layout.orderedKeys(present: present) {
            if present.contains(k) {
                try write(k, &self)
            } else if let raw = layout.unknown[k] {
                try encode(raw, forKey: AnyCodingKey(k))
            }
        }
    }

    mutating func put<T: Encodable>(_ value: T, _ key: String) throws {
        try encode(value, forKey: AnyCodingKey(key))
    }
}
