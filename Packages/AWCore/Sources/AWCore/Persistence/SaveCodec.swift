import Foundation

public enum SaveError: Error, Equatable, CustomStringConvertible {
    /// Saves before v10 need the prototype's migrations, which aren't ported (see docs/HANDOFF.md).
    case unsupportedVersion(Int)
    /// The prototype only loads saves with three or four locations.
    case unexpectedLocationCount(Int)
    case notASave(String)

    public var description: String {
        switch self {
        case .unsupportedVersion(let v):
            return "This save is version \(v). Open it in the web prototype once to update it to version \(AppState.currentVersion), then export again."
        case .unexpectedLocationCount(let n): return "This save has \(n) locations; expected 3 or 4."
        case .notASave(let why): return "This file isn't an Andrew's World save: \(why)"
        }
    }
}

/// Reads and writes the prototype's save format: the JSON `JSON.stringify(state)` produces.
public enum SaveCodec {
    /// Decodes a save exactly as written. Use `load(_:)` for the version checks and load-time fixes.
    public static func decode(_ data: Data) throws -> AppState {
        do {
            return try JSONValueDecoder().decode(AppState.self, from: data)
        } catch let e as JSONParseError {
            throw SaveError.notASave(e.description)
        } catch let e as DecodingError {
            throw SaveError.notASave(String(describing: e))
        }
    }

    /// Encodes the state byte for byte as the web prototype would save it.
    public static func encode(_ state: AppState) throws -> Data {
        try JSONValueEncoder().encodeToData(state)
    }

    /// Loads a save the way the prototype does on start-up: checks the version and location count, adds
    /// Solucan to a three-location save, then applies `normalize(_:)`.
    public static func load(_ data: Data) throws -> AppState {
        var state = try decode(data)
        guard state.v == AppState.currentVersion else { throw SaveError.unsupportedVersion(state.v) }
        guard state.locations.count == 3 || state.locations.count == 4 else {
            throw SaveError.unexpectedLocationCount(state.locations.count)
        }
        if state.locations.count == 3 {
            var solucan = try DefaultState.load().locations[3]
            renumberNotes(in: &solucan, from: state.nextNoteID)
            state.locations.append(solucan)
        }
        normalize(&state)
        return state
    }

    /// The fixes the prototype applies after loading any save: old location names, selection bounds.
    /// (Missing effect slots and part hues are filled in while decoding.)
    public static func normalize(_ state: inout AppState) {
        for i in state.locations.indices where ["Classic", "Cockpit"].contains(state.locations[i].name) {
            state.locations[i].name = state.locations[i].skin.displayName
        }
        state.currentLocation = min(max(0, state.currentLocation), state.locations.count - 1)
    }

    /// Gives every note in a location a fresh id, counting up from `first`.
    static func renumberNotes(in location: inout Location, from first: Int) {
        var next = first
        for s in location.songs.indices {
            for p in location.songs[s].parts.indices {
                for l in location.songs[s].parts[p].lanes.indices {
                    for b in location.songs[s].parts[p].lanes[l].bars.indices {
                        for n in location.songs[s].parts[p].lanes[l].bars[b].notes.indices {
                            location.songs[s].parts[p].lanes[l].bars[b].notes[n].id = next
                            next += 1
                        }
                    }
                }
            }
        }
    }
}

/// The prototype's demo state (`demo()`): four locations with their demo songs, exported from the prototype.
/// It is what a fresh install starts with, until the song generator is ported.
public enum DefaultState {
    public static func data() throws -> Data {
        guard let url = Bundle.module.url(forResource: "DefaultState", withExtension: "json") else {
            throw SaveError.notASave("DefaultState.json is missing from the bundle")
        }
        return try Data(contentsOf: url)
    }

    public static func load() throws -> AppState {
        try SaveCodec.decode(data())
    }
}
