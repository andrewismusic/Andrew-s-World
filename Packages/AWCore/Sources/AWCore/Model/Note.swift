import Foundation

/// How a ratchet's repeats move toward the note's ramp values.
public struct RampMode: RawRepresentable, Hashable, Codable, Sendable {
    public var rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    /// Repeats start at the note's sound and end at the ramp values.
    public static let away = RampMode(rawValue: "away")
    /// Repeats start at the ramp values and end at the note's sound.
    public static let toward = RampMode(rawValue: "toward")
}

/// One note in a bar. Pitch is a scale step (0–20 on melodic tracks) or a drum voice row (0–7, 0 = kick),
/// never a MIDI note: changing key or scale re-voices everything.
///
/// JSON: `{ id, step, pitch, len, locks, ramp, rm? }`
public struct Note: Equatable, Sendable {
    /// Unique across the whole state; the prototype uses it to light notes as they play.
    public var id: Int
    /// Grid step the note starts on: sixteenths in a straight bar, sixteenth triplets in a triplet bar.
    public var step: Int
    /// Scale step for melodic tracks, voice row for drums.
    public var pitch: Int
    /// Length in steps, at least 1.
    public var len: Int
    /// Per-note locks: the deepest level of the lock chain.
    public var locks: LockMap
    /// Where ratchet repeats ramp to (same keys as `locks`). Every prototype note has one, often empty.
    public var ramp: LockMap? {
        didSet { if ramp == nil { layout.forget("ramp") } }
    }
    /// Direction of the ramp; absent when the note has no ramp.
    public var rm: RampMode? {
        didSet { if rm == nil { layout.forget("rm") } }
    }

    public var layout = ObjectLayout()

    public init(id: Int, step: Int, pitch: Int, len: Int, locks: LockMap = [:], ramp: LockMap? = [:], rm: RampMode? = nil) {
        self.id = id
        self.step = step
        self.pitch = pitch
        self.len = len
        self.locks = locks
        self.ramp = ramp
        self.rm = rm
    }
}

extension Note: Codable {
    static let keys = ["id", "step", "pitch", "len", "locks", "ramp", "rm"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        id = try c.value("id")
        step = try c.value("step")
        pitch = try c.value("pitch")
        len = try c.value("len")
        locks = try c.optionalValue("locks") ?? [:]
        ramp = try c.optionalValue("ramp")
        rm = try c.optionalValue("rm")
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        var present = ["id", "step", "pitch", "len", "locks"]
        if ramp != nil { present.append("ramp") }
        if rm != nil { present.append("rm") }
        try c.writeLayout(layout, present: present) { k, c in
            switch k {
            case "id": try c.put(id, k)
            case "step": try c.put(step, k)
            case "pitch": try c.put(pitch, k)
            case "len": try c.put(len, k)
            case "locks": try c.put(locks, k)
            case "ramp": if let ramp { try c.put(ramp, k) }
            case "rm": if let rm { try c.put(rm, k) }
            default: break
            }
        }
    }
}
