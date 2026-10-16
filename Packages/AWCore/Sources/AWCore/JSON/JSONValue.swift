import Foundation

/// A JSON document as the prototype's JavaScript sees it: numbers are doubles and objects keep their key order.
///
/// Foundation's JSON tools drop key order and format numbers differently from `JSON.stringify`, so a save that
/// went through them would no longer match the web prototype byte for byte. This type, `JSONParser`,
/// `JSONWriter` and the `JSONValueEncoder`/`JSONValueDecoder` pair keep it exact.
public enum JSONValue: Equatable, Sendable {
    case null
    case bool(Bool)
    case number(Double)
    case string(String)
    case array([JSONValue])
    case object(JSONObject)
}

/// A JSON object that remembers insertion order, like a JavaScript object with string keys.
public struct JSONObject: Equatable, Sendable {
    public private(set) var keys: [String] = []
    private var values: [String: JSONValue] = [:]

    public init() {}

    public init(_ pairs: [(String, JSONValue)]) {
        for (k, v) in pairs { self[k] = v }
    }

    public var count: Int { keys.count }
    public var isEmpty: Bool { keys.isEmpty }

    /// Setting an existing key keeps its position; setting a new one appends it; setting nil removes it.
    public subscript(key: String) -> JSONValue? {
        get { values[key] }
        set {
            if let newValue {
                if values.updateValue(newValue, forKey: key) == nil { keys.append(key) }
            } else if values.removeValue(forKey: key) != nil {
                keys.removeAll { $0 == key }
            }
        }
    }

    public var pairs: [(key: String, value: JSONValue)] {
        keys.map { ($0, values[$0]!) }
    }

    public static func == (a: JSONObject, b: JSONObject) -> Bool {
        a.keys == b.keys && a.values == b.values
    }
}

extension JSONValue: Codable {
    public init(from decoder: Decoder) throws {
        if let d = decoder as? _JSONValueDecoder {
            self = d.value
            return
        }
        // Any other decoder: rebuild the tree through the standard containers (key order is then the decoder's).
        if var c = try? decoder.unkeyedContainer() {
            var out: [JSONValue] = []
            while !c.isAtEnd { out.append(try c.decode(JSONValue.self)) }
            self = .array(out)
        } else if let c = try? decoder.container(keyedBy: AnyCodingKey.self) {
            var o = JSONObject()
            for k in c.allKeys { o[k.stringValue] = try c.decode(JSONValue.self, forKey: k) }
            self = .object(o)
        } else {
            let c = try decoder.singleValueContainer()
            if c.decodeNil() { self = .null }
            else if let b = try? c.decode(Bool.self) { self = .bool(b) }
            else if let n = try? c.decode(Double.self) { self = .number(n) }
            else { self = .string(try c.decode(String.self)) }
        }
    }

    public func encode(to encoder: Encoder) throws {
        switch self {
        case .null:
            var c = encoder.singleValueContainer(); try c.encodeNil()
        case .bool(let b):
            var c = encoder.singleValueContainer(); try c.encode(b)
        case .number(let n):
            var c = encoder.singleValueContainer(); try c.encode(n)
        case .string(let s):
            var c = encoder.singleValueContainer(); try c.encode(s)
        case .array(let a):
            var c = encoder.unkeyedContainer()
            for v in a { try c.encode(v) }
        case .object(let o):
            var c = encoder.container(keyedBy: AnyCodingKey.self)
            for (k, v) in o.pairs { try c.encode(v, forKey: AnyCodingKey(k)) }
        }
    }
}

/// A coding key for any string, used where the keys are data (lock maps, unknown fields).
public struct AnyCodingKey: CodingKey, Hashable, Sendable {
    public var stringValue: String
    public var intValue: Int?
    public init(_ s: String) { stringValue = s; intValue = nil }
    public init?(stringValue: String) { self.init(stringValue) }
    public init?(intValue: Int) { stringValue = String(intValue); self.intValue = intValue }
}
