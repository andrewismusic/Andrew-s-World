import Foundation

/// Decodes any `Decodable` from an ordered `JSONValue`. Keyed containers list `allKeys` in document order,
/// so types can remember how their keys were laid out.
public struct JSONValueDecoder {
    public init() {}

    public func decode<T: Decodable>(_ type: T.Type, from value: JSONValue) throws -> T {
        if let v = value as? T { return v }
        return try T(from: _JSONValueDecoder(value: value, codingPath: []))
    }

    public func decode<T: Decodable>(_ type: T.Type, from data: Data) throws -> T {
        try decode(type, from: JSONParser.parse(data))
    }
}

final class _JSONValueDecoder: Decoder {
    let value: JSONValue
    let codingPath: [CodingKey]
    var userInfo: [CodingUserInfoKey: Any] { [:] }

    init(value: JSONValue, codingPath: [CodingKey]) {
        self.value = value
        self.codingPath = codingPath
    }

    func container<Key: CodingKey>(keyedBy type: Key.Type) throws -> KeyedDecodingContainer<Key> {
        guard case .object(let o) = value else { throw mismatch(value, "an object", codingPath) }
        return KeyedDecodingContainer(_KeyedDec<Key>(object: o, codingPath: codingPath))
    }

    func unkeyedContainer() throws -> UnkeyedDecodingContainer {
        guard case .array(let a) = value else { throw mismatch(value, "an array", codingPath) }
        return _UnkeyedDec(items: a, codingPath: codingPath)
    }

    func singleValueContainer() throws -> SingleValueDecodingContainer {
        _SingleDec(value: value, codingPath: codingPath)
    }

    static func decodeValue<T: Decodable>(_ type: T.Type, _ v: JSONValue, _ path: [CodingKey]) throws -> T {
        if T.self == JSONValue.self { return v as! T }
        if T.self == Double.self { return try asDouble(v, path) as! T }
        if T.self == Int.self { return try asInt(v, Int.self, path) as! T }
        if T.self == String.self { return try asString(v, path) as! T }
        if T.self == Bool.self { return try asBool(v, path) as! T }
        return try T(from: _JSONValueDecoder(value: v, codingPath: path))
    }
}

private func describe(_ v: JSONValue) -> String {
    switch v {
    case .null: return "null"
    case .bool: return "a boolean"
    case .number: return "a number"
    case .string: return "a string"
    case .array: return "an array"
    case .object: return "an object"
    }
}

private func mismatch(_ v: JSONValue, _ expected: String, _ path: [CodingKey]) -> DecodingError {
    .typeMismatch(Any.self, .init(codingPath: path, debugDescription: "Expected \(expected), found \(describe(v))"))
}

private func asBool(_ v: JSONValue, _ path: [CodingKey]) throws -> Bool {
    guard case .bool(let b) = v else { throw mismatch(v, "a boolean", path) }
    return b
}

private func asString(_ v: JSONValue, _ path: [CodingKey]) throws -> String {
    guard case .string(let s) = v else { throw mismatch(v, "a string", path) }
    return s
}

private func asDouble(_ v: JSONValue, _ path: [CodingKey]) throws -> Double {
    switch v {
    case .number(let d): return d
    case .null: return .nan  // JSON.stringify writes NaN as null; read it back as NaN so it writes out as null again
    default: throw mismatch(v, "a number", path)
    }
}

private func asInt<I: FixedWidthInteger>(_ v: JSONValue, _ type: I.Type, _ path: [CodingKey]) throws -> I {
    guard case .number(let d) = v else { throw mismatch(v, "a number", path) }
    guard d == d.rounded(), let i = I(exactly: d) else {
        throw DecodingError.dataCorrupted(.init(codingPath: path, debugDescription: "\(d) is not a whole number that fits \(I.self)"))
    }
    return i
}

private struct _KeyedDec<Key: CodingKey>: KeyedDecodingContainerProtocol {
    let object: JSONObject
    let codingPath: [CodingKey]

    var allKeys: [Key] { object.keys.compactMap { Key(stringValue: $0) } }
    func contains(_ key: Key) -> Bool { object[key.stringValue] != nil }

    private func get(_ key: Key) throws -> JSONValue {
        guard let v = object[key.stringValue] else {
            throw DecodingError.keyNotFound(key, .init(codingPath: codingPath, debugDescription: "No value for \"\(key.stringValue)\""))
        }
        return v
    }
    private func path(_ key: Key) -> [CodingKey] { codingPath + [key] }
    private func int<I: FixedWidthInteger>(_ key: Key) throws -> I { try asInt(get(key), I.self, path(key)) }

    func decodeNil(forKey key: Key) throws -> Bool { try get(key) == .null }
    func decode(_ type: Bool.Type, forKey key: Key) throws -> Bool { try asBool(get(key), path(key)) }
    func decode(_ type: String.Type, forKey key: Key) throws -> String { try asString(get(key), path(key)) }
    func decode(_ type: Double.Type, forKey key: Key) throws -> Double { try asDouble(get(key), path(key)) }
    func decode(_ type: Float.Type, forKey key: Key) throws -> Float { Float(try asDouble(get(key), path(key))) }
    func decode(_ type: Int.Type, forKey key: Key) throws -> Int { try int(key) }
    func decode(_ type: Int8.Type, forKey key: Key) throws -> Int8 { try int(key) }
    func decode(_ type: Int16.Type, forKey key: Key) throws -> Int16 { try int(key) }
    func decode(_ type: Int32.Type, forKey key: Key) throws -> Int32 { try int(key) }
    func decode(_ type: Int64.Type, forKey key: Key) throws -> Int64 { try int(key) }
    func decode(_ type: UInt.Type, forKey key: Key) throws -> UInt { try int(key) }
    func decode(_ type: UInt8.Type, forKey key: Key) throws -> UInt8 { try int(key) }
    func decode(_ type: UInt16.Type, forKey key: Key) throws -> UInt16 { try int(key) }
    func decode(_ type: UInt32.Type, forKey key: Key) throws -> UInt32 { try int(key) }
    func decode(_ type: UInt64.Type, forKey key: Key) throws -> UInt64 { try int(key) }
    func decode<T: Decodable>(_ type: T.Type, forKey key: Key) throws -> T {
        try _JSONValueDecoder.decodeValue(type, get(key), path(key))
    }

    func nestedContainer<NestedKey: CodingKey>(keyedBy type: NestedKey.Type, forKey key: Key) throws -> KeyedDecodingContainer<NestedKey> {
        try _JSONValueDecoder(value: get(key), codingPath: path(key)).container(keyedBy: type)
    }
    func nestedUnkeyedContainer(forKey key: Key) throws -> UnkeyedDecodingContainer {
        try _JSONValueDecoder(value: get(key), codingPath: path(key)).unkeyedContainer()
    }
    func superDecoder() throws -> Decoder {
        _JSONValueDecoder(value: object["super"] ?? .null, codingPath: codingPath + [AnyCodingKey("super")])
    }
    func superDecoder(forKey key: Key) throws -> Decoder {
        _JSONValueDecoder(value: object[key.stringValue] ?? .null, codingPath: path(key))
    }
}

private struct _UnkeyedDec: UnkeyedDecodingContainer {
    let items: [JSONValue]
    let codingPath: [CodingKey]
    var currentIndex = 0
    var count: Int? { items.count }
    var isAtEnd: Bool { currentIndex >= items.count }

    init(items: [JSONValue], codingPath: [CodingKey]) {
        self.items = items
        self.codingPath = codingPath
    }

    private var path: [CodingKey] { codingPath + [AnyCodingKey(intValue: currentIndex)!] }

    private mutating func next() throws -> JSONValue {
        guard !isAtEnd else {
            throw DecodingError.valueNotFound(Any.self, .init(codingPath: path, debugDescription: "Unkeyed container is at end"))
        }
        defer { currentIndex += 1 }
        return items[currentIndex]
    }
    private mutating func int<I: FixedWidthInteger>() throws -> I {
        let p = path
        return try asInt(next(), I.self, p)
    }

    mutating func decodeNil() throws -> Bool {
        guard !isAtEnd, items[currentIndex] == .null else { return false }
        currentIndex += 1
        return true
    }
    mutating func decode(_ type: Bool.Type) throws -> Bool { let p = path; return try asBool(next(), p) }
    mutating func decode(_ type: String.Type) throws -> String { let p = path; return try asString(next(), p) }
    mutating func decode(_ type: Double.Type) throws -> Double { let p = path; return try asDouble(next(), p) }
    mutating func decode(_ type: Float.Type) throws -> Float { let p = path; return Float(try asDouble(next(), p)) }
    mutating func decode(_ type: Int.Type) throws -> Int { try int() }
    mutating func decode(_ type: Int8.Type) throws -> Int8 { try int() }
    mutating func decode(_ type: Int16.Type) throws -> Int16 { try int() }
    mutating func decode(_ type: Int32.Type) throws -> Int32 { try int() }
    mutating func decode(_ type: Int64.Type) throws -> Int64 { try int() }
    mutating func decode(_ type: UInt.Type) throws -> UInt { try int() }
    mutating func decode(_ type: UInt8.Type) throws -> UInt8 { try int() }
    mutating func decode(_ type: UInt16.Type) throws -> UInt16 { try int() }
    mutating func decode(_ type: UInt32.Type) throws -> UInt32 { try int() }
    mutating func decode(_ type: UInt64.Type) throws -> UInt64 { try int() }
    mutating func decode<T: Decodable>(_ type: T.Type) throws -> T {
        let p = path
        return try _JSONValueDecoder.decodeValue(type, next(), p)
    }

    mutating func nestedContainer<NestedKey: CodingKey>(keyedBy type: NestedKey.Type) throws -> KeyedDecodingContainer<NestedKey> {
        let p = path
        return try _JSONValueDecoder(value: next(), codingPath: p).container(keyedBy: type)
    }
    mutating func nestedUnkeyedContainer() throws -> UnkeyedDecodingContainer {
        let p = path
        return try _JSONValueDecoder(value: next(), codingPath: p).unkeyedContainer()
    }
    mutating func superDecoder() throws -> Decoder {
        let p = path
        return _JSONValueDecoder(value: try next(), codingPath: p)
    }
}

private struct _SingleDec: SingleValueDecodingContainer {
    let value: JSONValue
    let codingPath: [CodingKey]

    private func int<I: FixedWidthInteger>() throws -> I { try asInt(value, I.self, codingPath) }

    func decodeNil() -> Bool { value == .null }
    func decode(_ type: Bool.Type) throws -> Bool { try asBool(value, codingPath) }
    func decode(_ type: String.Type) throws -> String { try asString(value, codingPath) }
    func decode(_ type: Double.Type) throws -> Double { try asDouble(value, codingPath) }
    func decode(_ type: Float.Type) throws -> Float { Float(try asDouble(value, codingPath)) }
    func decode(_ type: Int.Type) throws -> Int { try int() }
    func decode(_ type: Int8.Type) throws -> Int8 { try int() }
    func decode(_ type: Int16.Type) throws -> Int16 { try int() }
    func decode(_ type: Int32.Type) throws -> Int32 { try int() }
    func decode(_ type: Int64.Type) throws -> Int64 { try int() }
    func decode(_ type: UInt.Type) throws -> UInt { try int() }
    func decode(_ type: UInt8.Type) throws -> UInt8 { try int() }
    func decode(_ type: UInt16.Type) throws -> UInt16 { try int() }
    func decode(_ type: UInt32.Type) throws -> UInt32 { try int() }
    func decode(_ type: UInt64.Type) throws -> UInt64 { try int() }
    func decode<T: Decodable>(_ type: T.Type) throws -> T {
        try _JSONValueDecoder.decodeValue(type, value, codingPath)
    }
}
