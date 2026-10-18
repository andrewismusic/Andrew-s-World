import AWCore
import AWDSP
import Foundation
import Observation
#if canImport(MediaPlayer)
import MediaPlayer
#endif

/// Where the player is in the song tree, as the transport follows it.
public struct PlayContext: Equatable, Sendable {
    /// 0 song, 1 part, 2 track, 3 pattern, 4 note.
    public var level: Int
    public var song: Int
    public var part: Int
    public var bar: Int
    /// The pattern level's Loop switch.
    public var loop: Bool

    public init(level: Int, song: Int, part: Int = 0, bar: Int = 0, loop: Bool = false) {
        self.level = level
        self.song = song
        self.part = part
        self.bar = bar
        self.loop = loop
    }

    var native: aw_context {
        aw_context(level: Int32(level), song: Int32(song), part: Int32(part), bar: Int32(bar), loop: loop ? 1 : 0)
    }
}

/// The playhead, as of the last display frame.
public struct Playhead: Equatable, Sendable {
    public var song = 0, part = 0, bar = 0
    /// 12 ticks a quarter note.
    public var tick = 0
    public var barTicks = 48
}

/// Play, stop and the playhead, for the UI. Song data goes to the engine with `post` after every change.
@Observable
@MainActor
public final class Transport {
    public private(set) var isPlaying = false
    public private(set) var playhead = Playhead()
    /// Ids of notes that started since the last frame, for lighting them up.
    public private(set) var recentNotes: [Int32] = []

    @ObservationIgnored private let engine = AudioEngine()
    @ObservationIgnored private var timer: Timer?
    @ObservationIgnored private var context = PlayContext(level: 0, song: 0)
    @ObservationIgnored private var songName = ""

    public init() {
        setUpRemoteCommands()
    }

    public func post(_ state: AppState) {
        engine.post(state)
        let songs = state.locations[state.currentLocation].songs
        if songs.indices.contains(context.song) { songName = songs[context.song].name }
    }

    public func play(_ context: PlayContext) {
        self.context = context
        engine.play(context.native)
        isPlaying = true
        startPolling()
        updateNowPlaying()
    }

    public func stop() {
        engine.stop()
        isPlaying = false
        updateNowPlaying()
    }

    public func toggle() {
        if isPlaying { stop() } else { play(context) }
    }

    /// The selection or zoom level changed: the sequencer follows at the next bar line.
    public func follow(_ context: PlayContext) {
        guard context != self.context else { return }
        self.context = context
        engine.setContext(context.native)
    }

    public func cue(song: Int, part: Int, bar: Int) {
        engine.cue(song: song, part: part, bar: bar)
    }

    private func startPolling() {
        guard timer == nil else { return }
        let t = Timer(timeInterval: 1.0 / 30, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.poll() }
        }
        RunLoop.main.add(t, forMode: .common)
        timer = t
    }

    private func poll() {
        if let p = engine.playhead() {
            let next = Playhead(song: Int(p.song), part: Int(p.part), bar: Int(p.bar), tick: Int(p.tick), barTicks: Int(p.bar_ticks))
            if next != playhead { playhead = next }
        }
        let started = engine.takeStartedNotes()
        if !started.isEmpty || !recentNotes.isEmpty { recentNotes = started }
        if !isPlaying {
            timer?.invalidate()
            timer = nil
        }
    }

    // MARK: Lock screen and Control Center

    private func setUpRemoteCommands() {
        #if os(iOS)
        let center = MPRemoteCommandCenter.shared()
        center.playCommand.addTarget { [weak self] _ in
            MainActor.assumeIsolated { if let self, !self.isPlaying { self.play(self.context) } }
            return .success
        }
        center.pauseCommand.addTarget { [weak self] _ in
            MainActor.assumeIsolated { self?.stop() }
            return .success
        }
        center.togglePlayPauseCommand.addTarget { [weak self] _ in
            MainActor.assumeIsolated { self?.toggle() }
            return .success
        }
        #endif
    }

    private func updateNowPlaying() {
        #if os(iOS)
        MPNowPlayingInfoCenter.default().nowPlayingInfo = [
            MPMediaItemPropertyTitle: songName,
            MPMediaItemPropertyArtist: "Andrew's World",
            MPNowPlayingInfoPropertyPlaybackRate: isPlaying ? 1.0 : 0.0,
        ]
        MPNowPlayingInfoCenter.default().playbackState = isPlaying ? .playing : .paused
        #endif
    }
}
