import XCTest
@testable import AWCore

final class PersistenceTests: XCTestCase {
    var dir: URL!

    override func setUpWithError() throws {
        dir = FileManager.default.temporaryDirectory.appendingPathComponent("AWCoreTests-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    }

    override func tearDownWithError() throws {
        try? FileManager.default.removeItem(at: dir)
    }

    func testDefaultStateIsThePrototypeDemo() throws {
        let demo = try DefaultState.data()
        XCTAssertEqual(try SaveCodec.encode(DefaultState.load()), demo)
    }

    func testOlderVersionsAreRefusedWithAHelpfulError() throws {
        let json = #"{"v":9,"master":{"locks":{}},"wsel":0,"worlds":[],"sel":[0,1,0,null,0]}"#
        XCTAssertThrowsError(try SaveCodec.load(Data(json.utf8))) { e in
            XCTAssertEqual(e as? SaveError, .unsupportedVersion(9))
        }
    }

    func testNotJSONIsReportedAsNotASave() {
        XCTAssertThrowsError(try SaveCodec.load(Data("hello".utf8))) { e in
            guard case .notASave = e as? SaveError else { return XCTFail("\(e)") }
        }
    }

    func testThreeLocationSaveGetsSolucanWithFreshNoteIDs() throws {
        var s = try DefaultState.load()
        s.locations.removeLast()
        let maxID = s.allNotes.map(\.id).max()!
        let loaded = try SaveCodec.load(SaveCodec.encode(s))
        XCTAssertEqual(loaded.locations.map(\.skin), [.farm, .classic, .cockpit, .solucan])
        XCTAssertEqual(loaded.locations[3].songs.count, 4)
        let ids = loaded.allNotes.map(\.id)
        XCTAssertEqual(Set(ids).count, ids.count)
        XCTAssertTrue(loaded.locations[3].songs.allSatisfy { $0.parts.allSatisfy { $0.lanes.allSatisfy { $0.bars.allSatisfy { $0.notes.allSatisfy { $0.id > maxID } } } } })
    }

    func testOldLocationNamesAreRenamed() throws {
        var s = try DefaultState.load()
        s.locations[1].name = "Classic"
        s.locations[2].name = "Cockpit"
        let loaded = try SaveCodec.load(SaveCodec.encode(s))
        XCTAssertEqual(loaded.locations.map(\.name), ["Farm", "Blocks", "Laboratory", "Solucan"])
    }

    func testStoreStartsFreshThenLoadsWhatItSaved() async throws {
        let store = SaveStore(url: dir.appendingPathComponent("state.json"))
        var (state, outcome) = try await store.load()
        XCTAssertEqual(outcome, .fresh)
        state.locations[0].songs[0].name = "Renamed"
        await store.scheduleSave(state, delay: .milliseconds(10))
        try await store.flush()
        let (again, outcome2) = try await store.load()
        XCTAssertEqual(outcome2, .loaded)
        XCTAssertEqual(again.locations[0].songs[0].name, "Renamed")
        XCTAssertEqual(again, state)
    }

    func testDebouncedSaveWritesOnlyTheLatestState() async throws {
        let url = dir.appendingPathComponent("state.json")
        let store = SaveStore(url: url)
        var (state, _) = try await store.load()
        for i in 1...5 {
            state.locations[0].songs[0].name = "Take \(i)"
            await store.scheduleSave(state, delay: .milliseconds(50))
        }
        XCTAssertFalse(FileManager.default.fileExists(atPath: url.path), "nothing written while edits keep coming")
        // a debug build takes a moment to encode the whole demo; wait up to five seconds for the write
        for _ in 0..<50 where !FileManager.default.fileExists(atPath: url.path) {
            try await Task.sleep(for: .milliseconds(100))
        }
        XCTAssertEqual(try SaveCodec.decode(Data(contentsOf: url)).locations[0].songs[0].name, "Take 5")
    }

    func testUnreadableSaveIsMovedAsideNotOverwritten() async throws {
        let url = dir.appendingPathComponent("state.json")
        try Data("{broken".utf8).write(to: url)
        let store = SaveStore(url: url)
        let (_, outcome) = try await store.load()
        guard case .recovered(let backup, _) = outcome else { return XCTFail("\(outcome)") }
        XCTAssertEqual(try Data(contentsOf: backup), Data("{broken".utf8))
        XCTAssertFalse(FileManager.default.fileExists(atPath: url.path))
    }
}
