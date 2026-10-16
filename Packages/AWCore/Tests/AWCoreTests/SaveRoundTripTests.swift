import XCTest
@testable import AWCore

/// Milestone 1's "done when": a save exported from the web prototype round-trips byte-equivalent.
final class SaveRoundTripTests: XCTestCase {
    /// Saves exported from the prototype with `JSON.stringify(state)` (see tools/fixtures/README.md).
    /// `prototype-demo-save` is the untouched demo; `prototype-edited-save` adds a triplet bar, a ramp toward,
    /// mute, effects, master locks, a selected note, an awkward song name and a number JavaScript writes as 1e-7.
    static let fixtures = ["prototype-demo-save", "prototype-edited-save"]

    func testPrototypeSavesRoundTripByteForByte() throws {
        for name in Self.fixtures {
            let original = try fixture(name)
            let state = try SaveCodec.decode(original)
            let written = try SaveCodec.encode(state)
            assertSameBytes(written, original, name)
        }
    }

    /// The save you export from your own copy of the prototype. Skipped until reference/demo-save.json exists.
    func testReferenceDemoSaveRoundTrips() throws {
        let url = repoRoot().appendingPathComponent("reference/demo-save.json")
        guard FileManager.default.fileExists(atPath: url.path) else {
            throw XCTSkip("Add reference/demo-save.json (copy(JSON.stringify(state)) in the prototype) to run this test")
        }
        let original = try Data(contentsOf: url)
        let state = try SaveCodec.load(original)
        assertSameBytes(try SaveCodec.encode(state), original, "reference/demo-save.json")
    }

    func testRoundTripIsStableAcrossRepeatedSaves() throws {
        let original = try fixture("prototype-edited-save")
        var data = original
        for _ in 0..<3 { data = try SaveCodec.encode(SaveCodec.decode(data)) }
        assertSameBytes(data, original, "three round trips")
    }

    func testDecodedDemoMatchesThePrototype() throws {
        let s = try SaveCodec.load(fixture("prototype-demo-save"))
        XCTAssertEqual(s.v, 10)
        XCTAssertEqual(s.locations.map(\.skin), [.farm, .classic, .cockpit, .solucan])
        XCTAssertEqual(s.locations.map(\.name), ["Farm", "Blocks", "Laboratory", "Solucan"])
        XCTAssertEqual(s.locations.map(\.songs.count), [8, 4, 4, 4])
        XCTAssertEqual(s.sel, Selection(song: 0, part: 1, slot: 0, note: nil, bar: 0))

        let song = s.locations[0].songs[0]
        XCTAssertEqual(song.name, "Old School Hip-Hop")
        XCTAssertEqual(song.style, "hiphop")
        XCTAssertEqual(song.prog, [0, 3, 0, 4])
        XCTAssertEqual(song.parts.map(\.type), ["intro", "main", "alt", "main", "outro"])
        XCTAssertEqual(song.parts.map(\.length), [2, 4, 4, 4, 2])
        XCTAssertEqual(song.fx[0], [.mill, nil, nil])
        XCTAssertEqual(song.fx[2], [.well, nil, nil])
        XCTAssertEqual(song.locks["0.drums.d0.tu"], 0.42)
        XCTAssertTrue((0..<4).allSatisfy(song.mix.isAudible))
        XCTAssertTrue(song.parts.allSatisfy { $0.lanes.count == 4 })

        let first = song.parts[0].lanes[0].bars[0].notes[0]
        XCTAssertEqual(first.id, 1)
        XCTAssertEqual([first.step, first.pitch, first.len], [0, 0, 1])
        XCTAssertEqual(first.ramp, [:])
        XCTAssertNil(first.rm)

        let ids = s.allNotes.map(\.id)
        XCTAssertEqual(Set(ids).count, ids.count, "note ids are unique")
        XCTAssertEqual(s.nextNoteID, ids.max()! + 1)

        // pitches are scale steps / drum rows, never MIDI
        for loc in s.locations {
            for song in loc.songs {
                for part in song.parts {
                    for (slot, lane) in part.lanes.enumerated() {
                        let rows = slot == 0 ? 8 : 21
                        XCTAssertTrue(lane.bars.allSatisfy { $0.notes.allSatisfy { (0..<rows).contains($0.pitch) } })
                    }
                }
            }
        }
    }

    func testEditedFixtureFields() throws {
        let s = try SaveCodec.decode(fixture("prototype-edited-save"))
        XCTAssertEqual(s.currentLocation, 2)
        XCTAssertEqual(s.sel, Selection(song: 0, part: 1, slot: 1, note: 0, bar: 0))
        XCTAssertEqual(s.master.locks["g.bpm"], 1e-7)
        let song = s.locations[0].songs[0]
        XCTAssertEqual(song.name, "Ünïcode “quotes” \\ slash / tab\t")
        XCTAssertEqual(song.fx[1], [.mill, nil, .well])
        XCTAssertFalse(song.mix.isAudible(2))
        let bar = song.parts[1].lanes[1].bars[0]
        XCTAssertTrue(bar.isTriplet)
        XCTAssertEqual(bar.notes[0].rm, .toward)
        XCTAssertEqual(bar.notes[0].ramp?["1.bass.cf"], 0.123456789)
        XCTAssertEqual(bar.notes[0].locks["rat.rt"], 0.4)
    }

    // MARK: helpers

    func fixture(_ name: String) throws -> Data {
        let url = try XCTUnwrap(Bundle.module.url(forResource: name, withExtension: "json", subdirectory: "Fixtures"), "missing fixture \(name)")
        return try Data(contentsOf: url)
    }

    func repoRoot() -> URL {
        var u = URL(fileURLWithPath: #filePath)
        for _ in 0..<5 { u.deleteLastPathComponent() }  // file, AWCoreTests, Tests, AWCore, Packages
        return u
    }

    /// Compares two saves, ignoring trailing whitespace a paste may add, and points at the first difference.
    func assertSameBytes(_ a: Data, _ b: Data, _ label: String, file: StaticString = #filePath, line: UInt = #line) {
        let trim: (Data) -> [UInt8] = { d in
            var x = [UInt8](d)
            while let l = x.last, [0x20, 0x0A, 0x0D, 0x09].contains(l) { x.removeLast() }
            return x
        }
        let x = trim(a), y = trim(b)
        guard x != y else { return }
        let i = zip(x, y).firstIndex { $0 != $1 } ?? min(x.count, y.count)
        let around: ([UInt8]) -> String = { s in
            String(decoding: s[max(0, i - 80)..<min(s.count, i + 80)], as: UTF8.self)
        }
        XCTFail("\(label): differs at byte \(i) (lengths \(x.count) vs \(y.count))\nwritten:  …\(around(x))…\noriginal: …\(around(y))…", file: file, line: line)
    }
}
