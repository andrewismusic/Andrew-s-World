import Foundation

/// An effect building in one of a track's three effect slots.
public struct Building: RawRepresentable, Hashable, Codable, Sendable {
    public var rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    /// Distortion
    public static let mill = Building(rawValue: "mill")
    /// Echo
    public static let well = Building(rawValue: "well")
    /// Chorus
    public static let windmill = Building(rawValue: "windmill")
    /// Filter
    public static let barn = Building(rawValue: "barn")
    public static let all: [Building] = [.mill, .well, .windmill, .barn]
}

/// A mute or solo flag. Current saves store numbers (0/1); this keeps whichever form it was read in.
public enum TrackFlag: Equatable, Sendable, Codable {
    case number(Double)
    case bool(Bool)

    public var isOn: Bool {
        switch self {
        case .number(let n): return n != 0 && !n.isNaN
        case .bool(let b): return b
        }
    }

    public init(from decoder: Decoder) throws {
        let c = try decoder.singleValueContainer()
        if let b = try? c.decode(Bool.self) { self = .bool(b) } else { self = .number(try c.decode(Double.self)) }
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.singleValueContainer()
        switch self {
        case .number(let n): try c.encode(n)
        case .bool(let b): try c.encode(b)
        }
    }
}

/// Per-track mute (`m`) and solo (`s`), four each. JSON: `{ m: [4], s: [4] }`
public struct Mix: Equatable, Sendable {
    public var m: [TrackFlag]
    public var s: [TrackFlag]
    public var layout = ObjectLayout()

    public init(m: [TrackFlag] = Array(repeating: .number(0), count: 4), s: [TrackFlag] = Array(repeating: .number(0), count: 4)) {
        self.m = m
        self.s = s
    }

    /// Whether a track slot sounds: if anything is soloed only soloed tracks play, otherwise everything unmuted.
    public func isAudible(_ slot: Int) -> Bool {
        if s.contains(where: \.isOn) { return s.indices.contains(slot) && s[slot].isOn }
        return !(m.indices.contains(slot) && m[slot].isOn)
    }
}

extension Mix: Codable {
    static let keys = ["m", "s"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        m = try c.optionalValue("m") ?? Array(repeating: .number(0), count: 4)
        s = try c.optionalValue("s") ?? Array(repeating: .number(0), count: 4)
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            if k == "m" { try c.put(m, k) } else if k == "s" { try c.put(s, k) }
        }
    }
}

/// A song: its sound (locks), effects, mix and parts.
///
/// JSON: `{ name, style, hue, prog: [4 chord degrees], locks, mix, sections: [Part], fx: [4][3] building|null }`
public struct Song: Equatable, Sendable {
    public static let effectSlots = 3

    public var name: String
    /// Genre style key from the prototype's STYLES table (hiphop, trap, idm, house, dubstep, minimal, botanica, folk).
    public var style: String
    public var hue: Double
    /// Scale-degree chord progression used when generating bars, one degree per bar, cycling.
    public var prog: [Int]
    /// The song's sound: every track's parameters, score (`g.*`), space (`space.*`) and master (`master.*`).
    public var locks: LockMap
    public var mix: Mix
    /// Called `sections` in the save.
    public var parts: [Part]
    /// For each of the four tracks, three effect slots in series; nil is an empty slot.
    public var fx: [[Building?]]

    public var layout = ObjectLayout()

    public static let emptyFx: [[Building?]] = Array(repeating: Array(repeating: nil, count: effectSlots), count: 4)

    public init(name: String, style: String, hue: Double, prog: [Int], locks: LockMap, mix: Mix = Mix(), parts: [Part], fx: [[Building?]] = Song.emptyFx) {
        self.name = name
        self.style = style
        self.hue = hue
        self.prog = prog
        self.locks = locks
        self.mix = mix
        self.parts = parts
        self.fx = fx
    }
}

extension Song: Codable {
    static let keys = ["name", "style", "hue", "prog", "locks", "mix", "sections", "fx"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        name = try c.value("name")
        style = try c.optionalValue("style") ?? "house"
        hue = try c.optionalValue("hue") ?? 0
        prog = try c.optionalValue("prog") ?? [0, 0, 0, 0]
        locks = try c.optionalValue("locks") ?? [:]
        mix = try c.optionalValue("mix") ?? Mix()
        parts = try c.value("sections")
        // the prototype adds empty effect slots to songs saved before buildings existed
        fx = try c.optionalValue("fx") ?? Song.emptyFx
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            switch k {
            case "name": try c.put(name, k)
            case "style": try c.put(style, k)
            case "hue": try c.put(hue, k)
            case "prog": try c.put(prog, k)
            case "locks": try c.put(locks, k)
            case "mix": try c.put(mix, k)
            case "sections": try c.put(parts, k)
            case "fx": try c.put(fx, k)
            default: break
            }
        }
    }
}
