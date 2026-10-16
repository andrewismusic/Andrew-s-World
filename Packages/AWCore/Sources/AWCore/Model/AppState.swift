import Foundation

/// A location's look, and with it its engine set. Saves use the prototype's original skin names.
public struct Skin: RawRepresentable, Hashable, Codable, Sendable {
    public var rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public static let farm = Skin(rawValue: "farm")
    /// Shown as Blocks.
    public static let classic = Skin(rawValue: "classic")
    /// Shown as Laboratory.
    public static let cockpit = Skin(rawValue: "cockpit")
    public static let solucan = Skin(rawValue: "solucan")

    /// The location's name as the UI shows it.
    public var displayName: String {
        switch self {
        case .farm: return "Farm"
        case .classic: return "Blocks"
        case .cockpit: return "Laboratory"
        case .solucan: return "Solucan"
        default: return rawValue.capitalized
        }
    }

    /// The engine on each of the four track slots.
    public var engines: [Engine] {
        switch self {
        case .classic, .solucan: return [.drums, .bass, .chords, .prism]
        default: return [.drums, .bass, .chords, .lead]
        }
    }
}

public enum Engine: String, Sendable, CaseIterable {
    case drums, bass, chords, lead, prism
}

/// A location: a song library with its own look. JSON: `{ skin, name, songs: [Song] }`
public struct Location: Equatable, Sendable {
    public var skin: Skin
    public var name: String
    public var songs: [Song]
    public var layout = ObjectLayout()

    public init(skin: Skin, name: String, songs: [Song]) {
        self.skin = skin
        self.name = name
        self.songs = songs
    }
}

extension Location: Codable {
    static let keys = ["skin", "name", "songs"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        skin = try c.value("skin")
        name = try c.optionalValue("name") ?? skin.displayName
        songs = try c.value("songs")
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            switch k {
            case "skin": try c.put(skin, k)
            case "name": try c.put(name, k)
            case "songs": try c.put(songs, k)
            default: break
            }
        }
    }
}

/// The top of the lock chain: master → song → part → lane → note. JSON: `{ locks }`
public struct MasterNode: Equatable, Sendable {
    public var locks: LockMap
    public var layout = ObjectLayout()

    public init(locks: LockMap = [:]) { self.locks = locks }
}

extension MasterNode: Codable {
    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        locks = try c.optionalValue("locks") ?? [:]
        layout = try c.readLayout(known: ["locks"])
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: ["locks"]) { k, c in try c.put(locks, k) }
    }
}

/// Where the user is in the current location: `[song, part, slot, note | null, bar]`.
/// The bar is the page shown at the pattern level; the note is an index into that bar's notes.
public struct Selection: Equatable, Sendable, Codable {
    public var song: Int
    public var part: Int
    public var slot: Int
    public var note: Int?
    public var bar: Int

    public static let initial = Selection(song: 0, part: 1, slot: 0, note: nil, bar: 0)

    public init(song: Int, part: Int, slot: Int, note: Int?, bar: Int) {
        self.song = song
        self.part = part
        self.slot = slot
        self.note = note
        self.bar = bar
    }

    public init(from decoder: Decoder) throws {
        var c = try decoder.unkeyedContainer()
        func int() throws -> Int? {
            if c.isAtEnd { return nil }
            if try c.decodeNil() { return nil }
            return try c.decode(Int.self)
        }
        let d = Selection.initial
        song = try int() ?? d.song
        part = try int() ?? d.part
        slot = try int() ?? d.slot
        note = try int()
        bar = try int() ?? d.bar
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.unkeyedContainer()
        try c.encode(song)
        try c.encode(part)
        try c.encode(slot)
        if let note { try c.encode(note) } else { try c.encodeNil() }
        try c.encode(bar)
    }
}

/// Everything the prototype saves for songs: `{ v: 10, master, wsel, worlds: [Location], sel }`.
/// Preferences and lore are stored separately.
public struct AppState: Equatable, Sendable {
    public static let currentVersion = 10

    /// Save format version; the app reads 10.
    public var v: Int
    public var master: MasterNode
    /// Index of the current location (`wsel` in the save).
    public var currentLocation: Int
    /// Called `worlds` in the save; the UI renamed them, the format kept the key.
    public var locations: [Location]
    public var sel: Selection

    public var layout = ObjectLayout()

    public init(v: Int = AppState.currentVersion, master: MasterNode = MasterNode(), currentLocation: Int = 0, locations: [Location], sel: Selection = .initial) {
        self.v = v
        self.master = master
        self.currentLocation = currentLocation
        self.locations = locations
        self.sel = sel
    }

    /// The current location's songs (the prototype's `state.songs`).
    public var songs: [Song] {
        get { locations[currentLocation].songs }
        set { locations[currentLocation].songs = newValue }
    }

    /// Every note in every location.
    public var allNotes: [Note] {
        locations.flatMap { $0.songs.flatMap { $0.parts.flatMap { $0.lanes.flatMap { $0.bars.flatMap(\.notes) } } } }
    }

    /// The id the next new note should take: one past the highest in use, as the prototype computes on load.
    public var nextNoteID: Int { (allNotes.map(\.id).max() ?? 0) + 1 }
}

extension AppState: Codable {
    static let keys = ["v", "master", "wsel", "worlds", "sel"]

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: AnyCodingKey.self)
        v = try c.value("v")
        master = try c.optionalValue("master") ?? MasterNode()
        currentLocation = try c.optionalValue("wsel") ?? 0
        locations = try c.value("worlds")
        sel = (try? c.optionalValue("sel", as: Selection.self)) ?? .initial
        layout = try c.readLayout(known: Set(Self.keys))
    }

    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: AnyCodingKey.self)
        try c.writeLayout(layout, present: Self.keys) { k, c in
            switch k {
            case "v": try c.put(v, k)
            case "master": try c.put(master, k)
            case "wsel": try c.put(currentLocation, k)
            case "worlds": try c.put(locations, k)
            case "sel": try c.put(sel, k)
            default: break
            }
        }
    }
}
