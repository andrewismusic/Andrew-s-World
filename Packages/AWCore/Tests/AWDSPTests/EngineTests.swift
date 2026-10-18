import AWCore
import AWDSP
import XCTest

/// The sound engine through its C boundary, fed by the Swift encoder: the same path the app takes.
final class EngineTests: XCTestCase {
    private func farmJSON() throws -> (String, String) {
        let state = try DefaultState.load()
        let encoder = JSONValueEncoder()
        return (JSONWriter.string(try encoder.encode(state.locations[0])), JSONWriter.string(try encoder.encode(state.master)))
    }

    private func render(_ engine: OpaquePointer, seconds: Double, rate: Double) -> (left: [Float], right: [Float]) {
        let total = Int(seconds * rate), block = 512
        var left = [Float](repeating: 0, count: total), right = left
        left.withUnsafeMutableBufferPointer { l in
            right.withUnsafeMutableBufferPointer { r in
                var i = 0
                while i < total {
                    let n = min(block, total - i)
                    aw_engine_render(engine, Int32(n), l.baseAddress! + i, r.baseAddress! + i)
                    i += n
                }
            }
        }
        return (left, right)
    }

    func testDemoSongPlays() throws {
        let rate = 48000.0
        let engine = try XCTUnwrap(aw_engine_create(rate))
        defer { aw_engine_destroy(engine) }
        let (location, master) = try farmJSON()
        XCTAssertEqual(location.withCString { l in master.withCString { m in aw_engine_set_location(engine, l, m) } }, 0)
        aw_engine_play(engine, aw_context(level: 0, song: 0, part: 0, bar: 0, loop: 0))
        let out = render(engine, seconds: 6, rate: rate)

        XCTAssertTrue(out.left.allSatisfy(\.isFinite) && out.right.allSatisfy(\.isFinite))
        let peak = out.left.map(abs).max() ?? 0
        XCTAssertGreaterThan(peak, 0.05)
        XCTAssertLessThanOrEqual(peak, 0.65)

        let head = aw_engine_playhead(engine)
        XCTAssertEqual(head.playing, 1)
        // 6 s at 90 BPM is 2¼ bars: past the two-bar intro and into part B
        XCTAssertEqual(head.part, 1)
        XCTAssertEqual(head.bar, 0)
        XCTAssertEqual(head.time, 6, accuracy: 0.01)

        var ids = [Int32](repeating: 0, count: 64)
        let n = ids.withUnsafeMutableBufferPointer { aw_engine_take_started_notes(engine, $0.baseAddress, 64) }
        XCTAssertGreaterThan(n, 10)
    }

    func testStopLetsNotesRingOut() throws {
        let engine = try XCTUnwrap(aw_engine_create(44100))
        defer { aw_engine_destroy(engine) }
        let (location, master) = try farmJSON()
        _ = location.withCString { l in master.withCString { m in aw_engine_set_location(engine, l, m) } }
        aw_engine_play(engine, aw_context(level: 0, song: 0, part: 0, bar: 0, loop: 0))
        _ = render(engine, seconds: 1, rate: 44100)
        aw_engine_stop(engine)
        let tail = render(engine, seconds: 8, rate: 44100)
        XCTAssertEqual(aw_engine_playhead(engine).playing, 0)
        // reverb and delay tails fade to silence
        XCTAssertLessThan(tail.left.suffix(4410).map(abs).max() ?? 1, 0.001)
    }

    func testBadJSONIsRefused() throws {
        let engine = try XCTUnwrap(aw_engine_create(48000))
        defer { aw_engine_destroy(engine) }
        XCTAssertEqual(aw_engine_set_location(engine, "{\"songs\":", "{}"), -1)
    }
}
