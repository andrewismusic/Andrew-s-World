import Foundation

/// Encodes any `Encodable` into an ordered `JSONValue`. Keys come out in the order `encode(to:)` writes them,
/// which is what lets the model reproduce a prototype save byte for byte.
public struct JSONValueEncoder {
    public init() {}

    public func encode<T: Encodable>(_ value: T) throws -> JSONValue {
        try _JSONValueEncoder.encodeBox(value, codingPath: []).resolve()
    }

    /// Encodes and writes the result the way `JSON.stringify` does.
    public func encodeToData<T: Encodable>(_ value: T) throws -> Data {
        JSONWriter.data(try encode(value))
    }
}

/// A mutable node in the tree being built; containers hold references so nested containers can keep writing.
final class _EncodingBox {
    enum Kind { case unset, leaf(JSONValue), object, array }
    var kind: Kind = .unset
    var keys: [String] = []
    var children: [String: _EncodingBox] = [:]
    var items: [_EncodingBox] = []

    init() {}
    init(_ v: JSONValue) { kind = .leaf(v) }

    func set(_ key: String, _ box: _EncodingBox) {
        if children.updateValue(box, forKey: key) == nil { keys.append(key) }
    }

    func resolve() -> JSONValue {
        switch kind {
        case .unset: return .object(JSONObject())  // encoded nothing: an empty object, as JSONEncoder does
        case .leaf(let v): return v
        case .array: return .array(items.map { $0.resolve() })
        case .object:
            var o = JSONObject()
            for k in keys { o[k] = children[k]!.resolve() }
            return .object(o)
        }
    }
}

final class _JSONValueEncoder: Encoder {
    let box: _EncodingBox
    let codingPath: [CodingKey]
    var userInfo: [CodingUserInfoKey: Any] { [:] }

    init(box: _EncodingBox, codingPath: [CodingKey]) {
        self.box = box
        self.codingPath = codingPath
    }

    /// Encodes one value into a fresh box, short-cutting the common leaf types.
    static func encodeBox<T: Encodable>(_ value: T, codingPath: [CodingKey]) throws -> _EncodingBox {
        switch value {
        case let v as JSONValue: return _EncodingBox(v)
        case let v as Bool: return _EncodingBox(.bool(v))
        case let v as Int: return _EncodingBox(.number(Double(v)))
        case let v as Double: return _EncodingBox(.number(v))
        case let v as String: return _EncodingBox(.string(v))
        default:
            let b = _EncodingBox()
            try value.encode(to: _JSONValueEncoder(box: b, codingPath: codingPath))
            return b
        }
    }

    func container<Key: CodingKey>(keyedBy type: Key.Type) -> KeyedEncodingContainer<Key> {
        if case .unset = box.kind { box.kind = .object }
        return KeyedEncodingContainer(_KeyedEnc<Key>(box: box, codingPath: codingPath))
    }

    func unkeyedContainer() -> UnkeyedEncodingContainer {
        if case .unset = box.kind { box.kind = .array }
        return _UnkeyedEnc(box: box, codingPath: codingPath)
    }

    func singleValueContainer() -> SingleValueEncodingContainer {
        _SingleEnc(box: box, codingPath: codingPath)
    }
}

private struct _KeyedEnc<Key: CodingKey>: KeyedEncodingContainerProtocol {
    let box: _EncodingBox
    let codingPath: [CodingKey]

    private func put(_ v: JSONValue, _ key: Key) { box.set(key.stringValue, _EncodingBox(v)) }
    private func num<N: BinaryInteger>(_ v: N, _ key: Key) { put(.number(Double(v)), key) }

    mutating func encodeNil(forKey key: Key) throws { put(.null, key) }
    mutating func encode(_ value: Bool, forKey key: Key) throws { put(.bool(value), key) }
    mutating func encode(_ value: String, forKey key: Key) throws { put(.string(value), key) }
    mutating func encode(_ value: Double, forKey key: Key) throws { put(.number(value), key) }
    mutating func encode(_ value: Float, forKey key: Key) throws { put(.number(Double(value)), key) }
    mutating func encode(_ value: Int, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: Int8, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: Int16, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: Int32, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: Int64, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: UInt, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: UInt8, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: UInt16, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: UInt32, forKey key: Key) throws { num(value, key) }
    mutating func encode(_ value: UInt64, forKey key: Key) throws { num(value, key) }
    mutating func encode<T: Encodable>(_ value: T, forKey key: Key) throws {
        box.set(key.stringValue, try _JSONValueEncoder.encodeBox(value, codingPath: codingPath + [key]))
    }

    mutating func nestedContainer<NestedKey: CodingKey>(keyedBy keyType: NestedKey.Type, forKey key: Key) -> KeyedEncodingContainer<NestedKey> {
        let b = _EncodingBox()
        b.kind = .object
        box.set(key.stringValue, b)
        return KeyedEncodingContainer(_KeyedEnc<NestedKey>(box: b, codingPath: codingPath + [key]))
    }

    mutating func nestedUnkeyedContainer(forKey key: Key) -> UnkeyedEncodingContainer {
        let b = _EncodingBox()
        b.kind = .array
        box.set(key.stringValue, b)
        return _UnkeyedEnc(box: b, codingPath: codingPath + [key])
    }

    mutating func superEncoder() -> Encoder { superEncoder(forKey: Key(stringValue: "super")!) }

    mutating func superEncoder(forKey key: Key) -> Encoder {
        let b = _EncodingBox()
        box.set(key.stringValue, b)
        return _JSONValueEncoder(box: b, codingPath: codingPath + [key])
    }
}

private struct _UnkeyedEnc: UnkeyedEncodingContainer {
    let box: _EncodingBox
    let codingPath: [CodingKey]
    var count: Int { box.items.count }

    private func put(_ v: JSONValue) { box.items.append(_EncodingBox(v)) }
    private func num<N: BinaryInteger>(_ v: N) { put(.number(Double(v))) }
    private var nextKey: CodingKey { AnyCodingKey(intValue: box.items.count)! }

    mutating func encodeNil() throws { put(.null) }
    mutating func encode(_ value: Bool) throws { put(.bool(value)) }
    mutating func encode(_ value: String) throws { put(.string(value)) }
    mutating func encode(_ value: Double) throws { put(.number(value)) }
    mutating func encode(_ value: Float) throws { put(.number(Double(value))) }
    mutating func encode(_ value: Int) throws { num(value) }
    mutating func encode(_ value: Int8) throws { num(value) }
    mutating func encode(_ value: Int16) throws { num(value) }
    mutating func encode(_ value: Int32) throws { num(value) }
    mutating func encode(_ value: Int64) throws { num(value) }
    mutating func encode(_ value: UInt) throws { num(value) }
    mutating func encode(_ value: UInt8) throws { num(value) }
    mutating func encode(_ value: UInt16) throws { num(value) }
    mutating func encode(_ value: UInt32) throws { num(value) }
    mutating func encode(_ value: UInt64) throws { num(value) }
    mutating func encode<T: Encodable>(_ value: T) throws {
        box.items.append(try _JSONValueEncoder.encodeBox(value, codingPath: codingPath + [nextKey]))
    }

    mutating func nestedContainer<NestedKey: CodingKey>(keyedBy keyType: NestedKey.Type) -> KeyedEncodingContainer<NestedKey> {
        let path = codingPath + [nextKey]
        let b = _EncodingBox()
        b.kind = .object
        box.items.append(b)
        return KeyedEncodingContainer(_KeyedEnc<NestedKey>(box: b, codingPath: path))
    }

    mutating func nestedUnkeyedContainer() -> UnkeyedEncodingContainer {
        let path = codingPath + [nextKey]
        let b = _EncodingBox()
        b.kind = .array
        box.items.append(b)
        return _UnkeyedEnc(box: b, codingPath: path)
    }

    mutating func superEncoder() -> Encoder {
        let path = codingPath + [nextKey]
        let b = _EncodingBox()
        box.items.append(b)
        return _JSONValueEncoder(box: b, codingPath: path)
    }
}

private struct _SingleEnc: SingleValueEncodingContainer {
    let box: _EncodingBox
    let codingPath: [CodingKey]

    private func put(_ v: JSONValue) { box.kind = .leaf(v) }
    private func num<N: BinaryInteger>(_ v: N) { put(.number(Double(v))) }

    mutating func encodeNil() throws { put(.null) }
    mutating func encode(_ value: Bool) throws { put(.bool(value)) }
    mutating func encode(_ value: String) throws { put(.string(value)) }
    mutating func encode(_ value: Double) throws { put(.number(value)) }
    mutating func encode(_ value: Float) throws { put(.number(Double(value))) }
    mutating func encode(_ value: Int) throws { num(value) }
    mutating func encode(_ value: Int8) throws { num(value) }
    mutating func encode(_ value: Int16) throws { num(value) }
    mutating func encode(_ value: Int32) throws { num(value) }
    mutating func encode(_ value: Int64) throws { num(value) }
    mutating func encode(_ value: UInt) throws { num(value) }
    mutating func encode(_ value: UInt8) throws { num(value) }
    mutating func encode(_ value: UInt16) throws { num(value) }
    mutating func encode(_ value: UInt32) throws { num(value) }
    mutating func encode(_ value: UInt64) throws { num(value) }
    mutating func encode<T: Encodable>(_ value: T) throws {
        let b = try _JSONValueEncoder.encodeBox(value, codingPath: codingPath)
        box.kind = b.kind
        box.keys = b.keys
        box.children = b.children
        box.items = b.items
    }
}
