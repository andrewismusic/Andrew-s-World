import Foundation

/// A part of a song (A, B, intro…): four lanes, one per track slot.
///
/// JSON key in the save: an element of `song.sections`. JSON: `{ name, hue, type, locks, lanes: [Lane × 4] }`
public struct Part: Equatable, Sendable {
    /// A letter (A, B, …) by default.
    public var name: String
    /// 0–360; the part's colour on the map.
    public var hue: Double
    /// The style section it was generated from: intro, main, alt, break, outro.
    public var type: String
    public var locks: LockMap
    /// Always four, in slot order: drums, bass, chords, lead or prism.
    public var lanes: [Lane]

    public var layout = ObjectLayout()

    public init(name: String, hue: Double, type: String = "main", locks: LockMap = [:], lanes: [Lane]) {
        self.name = name
        self.hue = hue
        self.type = type
        self.locks = locks
        self.lanes = lanes
    }

    /// A part is as long as its longest lane, and at least one bar.
    public var length: Int { max(1, lanes.map(\.bars.count).max() ?? 0) }
}

extension Part: Codable {
    static let keys = ["name", "hue", "type", "locks", "lanes"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        name = try c.value("name")
        // the prototype gives a part without a hue a random one on load
        hue = try c.optionalValue("hue") ?? Double(Int.random(in: 0..<360))
        type = try c.optionalValue("type") ?? "main"
        locks = try c.optionalValue("locks") ?? [:]
        lanes = try c.value("lanes")
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            switch k {
            case "name": try c.put(name, k)
            case "hue": try c.put(hue, k)
            case "type": try c.put(type, k)
            case "locks": try c.put(locks, k)
            case "lanes": try c.put(lanes, k)
            default: break
            }
        }
    }
}

/// One track's row in a part. Lanes own their bars, so tracks can differ in length (up to `Lane.maxBars`).
///
/// JSON: `{ locks, bars: [Bar] }`
public struct Lane: Equatable, Sendable {
    public static let maxBars = 8

    public var locks: LockMap
    public var bars: [Bar]

    public var layout = ObjectLayout()

    public init(locks: LockMap = [:], bars: [Bar] = []) {
        self.locks = locks
        self.bars = bars
    }
}

extension Lane: Codable {
    static let keys = ["locks", "bars"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        locks = try c.optionalValue("locks") ?? [:]
        bars = try c.optionalValue("bars") ?? []
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            switch k {
            case "locks": try c.put(locks, k)
            case "bars": try c.put(bars, k)
            default: break
            }
        }
    }
}

/// One bar of one track: the pattern level.
///
/// JSON: `{ notes: [Note], trip?: true }`
public struct Bar: Equatable, Sendable {
    public var notes: [Note]
    /// Present (and true) on triplet bars. The prototype deletes the key rather than writing false.
    public var trip: Bool? {
        didSet { if trip == nil { layout.forget("trip") } }
    }

    public var layout = ObjectLayout()

    public init(notes: [Note] = [], trip: Bool? = nil) {
        self.notes = notes
        self.trip = trip
    }

    /// Triplet grid: six steps a beat instead of four. Turning it off removes the key, as the prototype does.
    public var isTriplet: Bool {
        get { trip ?? false }
        set { trip = newValue ? true : nil }
    }
}

extension Bar: Codable {
    static let keys = ["notes", "trip"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        notes = try c.optionalValue("notes") ?? []
        trip = try c.optionalValue("trip")
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: trip == nil ? ["notes"] : Self.keys) { k, c in
            switch k {
            case "notes": try c.put(notes, k)
            case "trip": if let trip { try c.put(trip, k) }
            default: break
            }
        }
    }
}
