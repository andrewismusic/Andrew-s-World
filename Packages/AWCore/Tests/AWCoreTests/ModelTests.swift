import XCTest
@testable import AWCore

final class ModelTests: XCTestCase {
    private func encode<T: Encodable>(_ v: T) throws -> String {
        String(decoding: try JSONValueEncoder().encodeToData(v), as: UTF8.self)
    }

    private func decode<T: Decodable>(_ t: T.Type, _ json: String) throws -> T {
        try JSONValueDecoder().decode(t, from: Data(json.utf8))
    }

    func testKeyOrderIsRememberedPerObject() throws {
        // newSong() in the prototype writes fx before locks; demo songs write it last. Both must survive.
        let json = #"{"name":"Untitled 1","style":"house","hue":300,"prog":[0,5,3,4],"fx":[[null,null,null],[null,null,null],[null,null,null],[null,null,null]],"locks":{},"mix":{"m":[0,0,0,0],"s":[0,0,0,0]},"sections":[]}"#
        XCTAssertEqual(try encode(decode(Song.self, json)), json)
    }

    func testNewKeysGoLastLikeJavaScript() throws {
        var bar = try decode(Bar.self, #"{"notes":[]}"#)
        bar.isTriplet = true
        XCTAssertEqual(try encode(bar), #"{"notes":[],"trip":true}"#)
        bar.isTriplet = false
        XCTAssertEqual(try encode(bar), #"{"notes":[]}"#, "turning triplets off deletes the key")

        var note = try decode(Note.self, #"{"id":7,"step":2,"pitch":9,"len":1,"locks":{"1.bass.cf":0.5},"ramp":{}}"#)
        note.locks["rat.rt"] = 0.4
        note.locks["1.bass.cf"] = 0.25
        note.rm = .away
        XCTAssertEqual(try encode(note), #"{"id":7,"step":2,"pitch":9,"len":1,"locks":{"1.bass.cf":0.25,"rat.rt":0.4},"ramp":{},"rm":"away"}"#)
        note.locks["1.bass.cf"] = nil
        note.rm = nil
        XCTAssertEqual(try encode(note), #"{"id":7,"step":2,"pitch":9,"len":1,"locks":{"rat.rt":0.4},"ramp":{}}"#)
    }

    func testDeletedKeyReturnsAtTheEnd() throws {
        var note = try decode(Note.self, #"{"id":1,"step":0,"pitch":0,"len":1,"locks":{},"rm":"away","ramp":{"0.drums.d0.lv":0.5}}"#)
        note.rm = nil
        note.rm = .toward
        XCTAssertEqual(try encode(note), #"{"id":1,"step":0,"pitch":0,"len":1,"locks":{},"ramp":{"0.drums.d0.lv":0.5},"rm":"toward"}"#)
    }

    func testUnknownKeysSurvive() throws {
        let json = #"{"skin":"farm","future":{"b":1,"a":[true]},"name":"Farm","songs":[],"zzz":null}"#
        let loc = try decode(Location.self, json)
        XCTAssertEqual(loc.layout.unknown.keys, ["future", "zzz"])
        XCTAssertEqual(try encode(loc), json)
    }

    func testMixFlagsKeepTheirForm() throws {
        let json = #"{"m":[0,1,false,true],"s":[0,0,0,0]}"#
        let mix = try decode(Mix.self, json)
        XCTAssertEqual(try encode(mix), json)
        XCTAssertEqual((0..<4).map(mix.isAudible), [true, false, true, false])
        let solo = try decode(Mix.self, #"{"m":[1,0,0,0],"s":[1,0,0,0]}"#)
        XCTAssertEqual((0..<4).map(solo.isAudible), [true, false, false, false], "solo wins over mute")
    }

    func testSelectionIsAFiveElementArray() throws {
        XCTAssertEqual(try encode(Selection.initial), "[0,1,0,null,0]")
        XCTAssertEqual(try decode(Selection.self, "[2,3,1,4,5]"), Selection(song: 2, part: 3, slot: 1, note: 4, bar: 5))
    }

    func testPartLengthIsTheLongestLane() throws {
        let p = Part(name: "A", hue: 10, lanes: [Lane(bars: [Bar(), Bar()]), Lane(bars: [Bar(), Bar(), Bar()]), Lane(), Lane()])
        XCTAssertEqual(p.length, 3)
        XCTAssertEqual(Part(name: "B", hue: 0, lanes: [Lane(), Lane(), Lane(), Lane()]).length, 1)
    }

    func testSkinEngines() {
        XCTAssertEqual(Skin.farm.engines, [.drums, .bass, .chords, .lead])
        XCTAssertEqual(Skin.classic.engines.last, .prism)
        XCTAssertEqual(Skin.solucan.engines.last, .prism)
        XCTAssertEqual(Skin.cockpit.displayName, "Laboratory")
    }
}
