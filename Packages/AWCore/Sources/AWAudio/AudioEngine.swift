import AVFoundation
import AWCore
import AWDSP
import Foundation

/// Owns the device audio (AVAudioEngine with one source node) and the native sound engine behind it.
///
/// Every call into the native engine except rendering happens on one serial queue, so song data and transport
/// commands each come from a single thread, as AWDSP requires. The render callback only calls `aw_engine_render`.
final class AudioEngine: @unchecked Sendable {
    private let queue = DispatchQueue(label: "andrewsworld.audio", qos: .userInitiated)
    private let avEngine = AVAudioEngine()
    private var source: AVAudioSourceNode?
    private var native: OpaquePointer?
    private var sampleRate: Double = 0
    private let lock = NSLock()

    // what to restore after a rebuild
    private var lastSongData: (location: String, master: String)?
    private var postedGeneration = 0
    private var latestGeneration = 0
    private var playingContext: aw_context?

    init() {
        queue.async { self.start() }
        let center = NotificationCenter.default
        center.addObserver(forName: .AVAudioEngineConfigurationChange, object: avEngine, queue: nil) { [weak self] _ in
            self?.queue.async { self?.rebuild() }
        }
        #if os(iOS)
        center.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  AVAudioSession.InterruptionType(rawValue: raw) == .ended else { return }
            self?.queue.async { self?.restartIfNeeded() }
        }
        center.addObserver(forName: AVAudioSession.mediaServicesWereResetNotification, object: nil, queue: nil) { [weak self] _ in
            self?.queue.async { self?.rebuild() }
        }
        center.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil, queue: nil) { [weak self] _ in
            self?.queue.async { self?.rebuildIfRateChanged() }
        }
        #endif
    }

    // MARK: Called from the app (any thread)

    /// Posts the current location's songs. Only the latest of several quick posts is built.
    func post(_ state: AppState) {
        lock.lock()
        latestGeneration += 1
        let generation = latestGeneration
        lock.unlock()
        queue.async {
            self.lock.lock()
            let stale = generation != self.latestGeneration
            self.lock.unlock()
            if stale { return }
            guard let data = try? SongData.json(for: state) else { return }
            self.lastSongData = data
            self.send(data)
        }
    }

    func play(_ context: aw_context) {
        queue.async {
            self.playingContext = context
            self.restartIfNeeded()
            if let e = self.native { aw_engine_play(e, context) }
        }
    }

    func stop() {
        queue.async {
            self.playingContext = nil
            if let e = self.native { aw_engine_stop(e) }
        }
    }

    func setContext(_ context: aw_context) {
        queue.async {
            if self.playingContext != nil { self.playingContext = context }
            if let e = self.native { aw_engine_set_context(e, context) }
        }
    }

    func cue(song: Int, part: Int, bar: Int) {
        queue.async {
            if let e = self.native { aw_engine_cue(e, Int32(song), Int32(part), Int32(bar)) }
        }
    }

    /// Safe from any thread; nil while the engine is being rebuilt.
    func playhead() -> aw_playhead? {
        lock.lock()
        defer { lock.unlock() }
        guard let e = native else { return nil }
        return aw_engine_playhead(e)
    }

    func takeStartedNotes() -> [Int32] {
        lock.lock()
        defer { lock.unlock() }
        guard let e = native else { return [] }
        var ids = [Int32](repeating: 0, count: 256)
        let n = ids.withUnsafeMutableBufferPointer { aw_engine_take_started_notes(e, $0.baseAddress, 256) }
        return Array(ids.prefix(Int(n)))
    }

    // MARK: On the queue

    private func send(_ data: (location: String, master: String)) {
        guard let e = native else { return }
        _ = data.location.withCString { l in data.master.withCString { m in aw_engine_set_location(e, l, m) } }
    }

    private func configureSession() {
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .default)
        try? session.setPreferredIOBufferDuration(0.005)
        try? session.setActive(true)
        #endif
    }

    private func start() {
        configureSession()
        let rate = avEngine.outputNode.outputFormat(forBus: 0).sampleRate
        sampleRate = rate > 0 ? rate : 48000
        guard let e = aw_engine_create(sampleRate) else { return }
        let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2)!
        let node = AVAudioSourceNode(format: format) { _, _, frameCount, bufferList -> OSStatus in
            let buffers = UnsafeMutableAudioBufferListPointer(bufferList)
            guard buffers.count >= 2,
                  let left = buffers[0].mData?.assumingMemoryBound(to: Float.self),
                  let right = buffers[1].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            aw_engine_render(e, Int32(frameCount), left, right)
            return noErr
        }
        avEngine.attach(node)
        avEngine.connect(node, to: avEngine.mainMixerNode, format: format)
        lock.lock()
        native = e
        source = node
        lock.unlock()
        if let data = lastSongData { send(data) }
        try? avEngine.start()
        if let context = playingContext { aw_engine_play(e, context) }
    }

    private func tearDown() {
        avEngine.stop()
        if let node = source { avEngine.detach(node) }
        lock.lock()
        let e = native
        native = nil
        source = nil
        lock.unlock()
        if let e { aw_engine_destroy(e) }
    }

    private func rebuild() {
        tearDown()
        start()
    }

    private func rebuildIfRateChanged() {
        let rate = avEngine.outputNode.outputFormat(forBus: 0).sampleRate
        if rate > 0 && rate != sampleRate { rebuild() } else { restartIfNeeded() }
    }

    private func restartIfNeeded() {
        if native == nil { start(); return }
        if !avEngine.isRunning {
            configureSession()
            try? avEngine.start()
        }
    }
}
